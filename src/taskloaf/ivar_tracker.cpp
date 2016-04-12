#include "ivar_tracker.hpp"
#include "id.hpp"
#include "comm.hpp"
#include "ring.hpp"
#include "protocol.hpp"
#include "ivar_db.hpp"

#include <unordered_map>
#include <set>
#include <iostream>

namespace taskloaf {

struct IVarTrackerImpl {
    Comm& comm;
    Ring ring;
    IVarDB db;

    IVarTrackerImpl(Comm& comm):
        comm(comm),
        ring(comm, 1)
    {
        setup_handlers();
    }



    template <typename F>
    void forward_or(const ID& id, F f) {
        auto owner = ring.get_owner(id);
        if (!is_local(owner)) {
            comm.send(owner, comm.cur_message());
            return;
        }
        f();
    }

    typedef std::pair<ID,IVarOwnershipData> Transfer;

    void setup_handlers() {
        comm.add_handler(Protocol::InitiateTransfer, [&] (Data d) {
            auto& p = d.get_as<std::tuple<ID,ID,Address>>();
            std::vector<Transfer> transfer;
            std::vector<ID> erase_me;
            for (auto& ivar: db) {
                if (in_interval(std::get<0>(p), std::get<1>(p), ivar.first)) {
                    transfer.emplace_back(ivar.first, std::move(ivar.second.ownership));
                    ivar.second.ownership.ref_count.zero();
                    if (ivar.second.triggers.size() == 0 &&
                            ivar.second.vals.size() == 0) {
                        erase_me.push_back(ivar.first);
                    }
                }
            }
            for (auto& id: erase_me) {
                db.erase(id);
            }
            comm.send(std::get<2>(p), Msg(Protocol::SendOwnership,
                make_data(std::move(transfer))
            ));
        });

        comm.add_handler(Protocol::SendOwnership, [&] (Data d) {
            auto& transfer = d.get_as<std::vector<std::pair<ID,IVarOwnershipData>>>();
            std::map<Address,std::vector<Transfer>> resends;
            for (auto& t: transfer) {
                auto owner = ring.get_owner(t.first);
                if (is_local(owner)) {
                    merge_ownership(t.first, t.second);
                } else {
                    resends[owner].push_back(t); 
                }
            }
            for (auto& r: resends) {
                comm.send(r.first, Msg(Protocol::SendOwnership,
                    make_data(std::move(r.second))
                ));
            }
        });

        // On the owner
        comm.add_handler(Protocol::DecRef, [&] (Data d) {
            auto& iv = d.get_as<std::pair<ID,RefData>>();

            forward_or(iv.first, [&] () {
                local_dec_ref(iv.first, iv.second);
            });
        });
        comm.add_handler(Protocol::Fulfill, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,Address>>();
            auto& iv = p.first;

            forward_or(iv.id, [&] () {
                db.add_val_loc(iv.id, p.second);

                auto& trigger_locs = db[iv.id].ownership.trigger_locs;
                comm.send(p.second, Msg(Protocol::TriggerLocs, make_data(
                    std::make_pair(std::move(iv), std::move(trigger_locs))
                )));
            });
        });
        comm.add_handler(Protocol::AddTrigger, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,Address>>();
            auto& iv = p.first;

            forward_or(iv.id, [&] () {
                if (db.is_fulfilled(iv.id)) {
                    assert(db[iv.id].ownership.trigger_locs.size() == 0);
                    auto& val_loc = *db[iv.id].ownership.val_locs.begin();
                    comm.send(val_loc, Msg(Protocol::TriggerLocs, make_data(
                        std::make_pair(std::move(iv), std::set<Address>{p.second})
                    )));
                } else {
                    db.add_trigger_loc(iv.id, p.second);
                }
            });
        });

        // On the fulfiller
        comm.add_handler(Protocol::TriggerLocs, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,std::set<Address>>>();
            auto& iv = p.first;
            assert(db[iv.id].vals.size() > 0);
            fulfill_triggers(iv, p.second);
        });
        comm.add_handler(Protocol::RecvTriggers, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,std::vector<TriggerT>>>();
            auto& iv = p.first;
            db.run_remote_triggers(iv.id, std::move(p.second));
        });
        comm.add_handler(Protocol::DeleteInfo, [&] (Data d) {
            auto& id = d.get_as<ID>();
            assert(db.contains(id));
            db.erase(id);
        });
        
        // On where add_trigger was called.
        comm.add_handler(Protocol::GetTriggers, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,Address>>();
            auto& iv = p.first;
            auto& trigs_to_send = db[iv.id].triggers;
            comm.send(p.second, Msg(Protocol::RecvTriggers, make_data(
                std::make_pair(std::move(iv), std::move(trigs_to_send))
            )));
            if (!is_local(ring.get_owner(iv.id)) && db[iv.id].vals.size() == 0) {
                db.erase(iv.id);
            }
        });
    }

    void merge_ownership(const ID& id, const IVarOwnershipData& other) {
        auto& ownership = db[id].ownership;
        for (auto& loc: other.val_locs) {
            ownership.val_locs.insert(loc);
        }
        for (auto& loc: other.trigger_locs) {
            ownership.trigger_locs.insert(loc);
        }

        // Maintain trigger calling invariant:
        // val_locs.size() == 0 || trigger_locs.size() == 0
        if (ownership.val_locs.size() > 0 && ownership.trigger_locs.size() > 0) {
            // Need to create a new IvarRef here or the data may be deleted 
            // because the ref used to create the trigger and the ref used
            // to fulfill the ivar may both be dead.
            auto iv = IVarRef(id, copy_ref(ownership.ref_count.source_ref));
            auto v_loc = *ownership.val_locs.begin();
            comm.send(v_loc, Msg(Protocol::TriggerLocs, make_data(
                std::make_pair(std::move(iv), std::move(ownership.trigger_locs))
            )));
        }

        ownership.ref_count.merge(other.ref_count);
        // Maintain reference counting invariant:
        // ref_count.counts[i] == 0 for all i --> delete
        if (ownership.ref_count.dead()) {
            erase(id); 
        }
    }

    void local_dec_ref(const ID& id, const RefData& ref_data) {
        auto& rc = db[id].ownership.ref_count;
        rc.dec(ref_data);
        if (rc.dead()) {
            erase(id);
        }
    }

    void erase(const ID& id) {
        assert(is_local(ring.get_owner(id)));
        auto& info = db[id];
        for (auto& v_loc: info.ownership.val_locs) {
            if (!is_local(v_loc)) {
                comm.send(v_loc, Msg(Protocol::DeleteInfo, make_data(id)));
            }
        }
        for (auto& t_loc: info.ownership.trigger_locs) {
            if (!is_local(t_loc)) {
                comm.send(t_loc, Msg(Protocol::DeleteInfo, make_data(id)));
            }
        }
        db.erase(id);
    }

    void fulfill_triggers(const IVarRef& iv, const std::set<Address>& trigger_locs) {
        for (auto& t_loc: trigger_locs) {
            if (is_local(t_loc)) {
                db.run_local_triggers(iv.id, db[iv.id].vals);
            } else {
                comm.send(t_loc, Msg(Protocol::GetTriggers, make_data(
                    std::make_pair(iv, comm.get_addr())
                )));
            }
        }
    }

    void local_fulfill(const IVarRef& iv) {
        db.add_val_loc(iv.id, comm.get_addr());
        fulfill_triggers(iv, db[iv.id].ownership.trigger_locs);
        db[iv.id].ownership.trigger_locs.clear();
    }

    void local_add_trigger(const IVarRef& iv) {
        if (!db.is_fulfilled(iv.id)) {
            db.add_trigger_loc(iv.id, comm.get_addr());
            return;
        }

        auto& info = db[iv.id];
        auto& val_loc = *info.ownership.val_locs.begin();
        if (is_local(val_loc)) {
            db.run_local_triggers(iv.id, info.vals);
        } else {
            comm.send(val_loc, Msg(Protocol::RecvTriggers, make_data(
                std::make_pair(iv, std::move(info.triggers))
            )));
        }
    }

    bool is_local(const Address& addr) {
        return addr == comm.get_addr();
    }
};


IVarTracker::IVarTracker(Comm& comm):
    impl(std::make_unique<IVarTrackerImpl>(comm))
{}

IVarTracker::IVarTracker(IVarTracker&&) = default;

IVarTracker::~IVarTracker() = default;

void IVarTracker::fulfill(const IVarRef& iv, std::vector<Data> input) {
    assert(input.size() > 0);
    assert(impl->db[iv.id].vals.size() == 0);
    impl->db[iv.id].vals = std::move(input);

    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_fulfill(iv);
    } else {
        impl->comm.send(owner, Msg(Protocol::Fulfill, make_data(
            std::make_pair(iv, impl->comm.get_addr())
        )));
    }
}

void IVarTracker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    impl->db[iv.id].triggers.push_back(std::move(trigger));
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_add_trigger(iv);
    } else {
        impl->comm.send(owner, Msg(Protocol::AddTrigger, make_data(
            std::make_pair(iv, impl->comm.get_addr())
        )));
    }
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_dec_ref(iv.id, iv.data);
    } else {
        impl->comm.send(owner, Msg(Protocol::DecRef, make_data(
            std::make_pair(iv.id, iv.data)
        )));
    }
}

void IVarTracker::introduce(Address addr) {
    impl->ring.introduce(addr);
}

size_t IVarTracker::n_owned() const {
    size_t out = 0;
    for (auto& o: impl->db) {
        auto owner = impl->ring.get_owner(o.first);
        if (impl->is_local(owner)) {
            out++;
        }
    }
    return out;
}

size_t IVarTracker::n_triggers_here() const {
    size_t out = 0;
    for (auto& o: impl->db) {
        out += o.second.triggers.size();
    }
    return out;
}

size_t IVarTracker::n_vals_here() const {
    size_t out = 0;
    for (auto& o: impl->db) {
        out += o.second.vals.size();
    }
    return out;
}

const std::vector<ID>& IVarTracker::get_ring_locs() const {
    return impl->ring.get_locs();
}

std::vector<Address> IVarTracker::ring_members() const {
    return impl->ring.ring_members();
}

} //end namespace taskloaf

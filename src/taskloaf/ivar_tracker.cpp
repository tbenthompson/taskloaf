#include "ivar_tracker.hpp"
#include "id.hpp"
#include "comm.hpp"
#include "ring.hpp"
#include "protocol.hpp"

#include <unordered_map>
#include <set>
#include <iostream>

namespace taskloaf {

struct IVarOwnershipData {
    int ref_count;
    std::unique_ptr<Address> val_loc;
    std::set<Address> trigger_locs;
    //TODO: Also store value size in bytes. For greedy memory transfer.
    //TODO: Also store trigger sizes in bytes. For greedy memory transfer.
};

struct IVarData {
    std::vector<Data> vals;
    std::vector<TriggerT> triggers;
    IVarOwnershipData ownership;
};

void run_triggers(std::vector<TriggerT>& triggers, std::vector<Data>& vals) {
    // We can't just do a for loop over the currently present triggers
    // because a trigger on a given IVar could add another trigger on
    // the same IVar.
    while (triggers.size() > 0) {
        auto t = std::move(triggers.back());
        triggers.pop_back();
        t(vals);
    }
    triggers.clear();
}

struct IVarDB {
    std::unordered_map<ID,IVarData> db;

    auto& operator[](const ID& id) {
        return db[id];
    }

    void erase(const ID& id) {
        db.erase(id);
    }

    bool contains(const ID& id) {
        return db.count(id) > 0;
    }

    auto begin() {
        return db.begin();
    }

    auto end() {
        return db.end();
    }

    void set_val_loc(const ID& id, const Address& loc) {
        assert(db[id].ownership.val_loc == nullptr);
        db[id].ownership.val_loc = std::make_unique<Address>(loc);
    }

    void add_trigger_loc(const ID& id, const Address& loc) {
        db[id].ownership.trigger_locs.insert(loc);
    }

    bool is_fulfilled(const ID& id) {
        return db[id].ownership.val_loc != nullptr;
    }

    void run_remote_triggers(const ID& id, std::vector<TriggerT> triggers) {
        taskloaf::run_triggers(triggers, db[id].vals);
    }

    void run_local_triggers(const ID& id, std::vector<Data>& input) {
        taskloaf::run_triggers(db[id].triggers, input);
    }
};

struct IVarTrackerImpl {
    Comm& comm;
    Ring ring;
    IVarDB db;

    IVarTrackerImpl(Comm& comm):
        comm(comm),
        ring(comm, 1)
    {
        ring.add_transfer_handler([&] (ID begin, ID end, Address new_owner) {
            std::vector<std::pair<ID,IVarOwnershipData>> transfer;
            for (auto& ivar: db.db) {
                if (begin <= ivar.first && ivar.first < end) {
                    transfer.emplace_back(ivar.first, std::move(ivar.second.ownership));
                }
                ivar.second.ownership.ref_count = 0;
            }
            comm.send(new_owner, Msg(Protocol::OwnershipTransfer,
                make_data(std::move(transfer))
            ));
        });
        setup_handlers();
    }

    ~IVarTrackerImpl() {
    }

    template <typename F>
    void forward_or(const ID& id, F f) {
        auto owner = ring.get_owner(id);
        if (!is_local(owner)) {
            comm.forward(owner);
            return;
        }
        f();
    }

    void setup_handlers() {
        comm.add_handler(Protocol::OwnershipTransfer, [&] (Data d) {
            auto& transfer = d.get_as<std::vector<std::pair<ID,IVarOwnershipData>>>();
            for (auto& t: transfer) {
                db.db[t.first].ownership = std::move(t.second);
            }
        });

        // On the owner
        comm.add_handler(Protocol::IncRef, [&] (Data d) {
            auto& id = d.get_as<ID>();

            forward_or(id, [&] () {
                local_inc_ref(id);
            });
        });
        comm.add_handler(Protocol::DecRef, [&] (Data d) {
            auto& id = d.get_as<ID>();

            forward_or(id, [&] () {
                local_dec_ref(id);
            });
        });
        comm.add_handler(Protocol::Fulfill, [&] (Data d) {
            auto& p = d.get_as<std::pair<IVarRef,Address>>();
            auto& iv = p.first;

            forward_or(iv.id, [&] () {
                db.set_val_loc(iv.id, p.second);

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
                    auto& val_loc = *db[iv.id].ownership.val_loc;
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
        });
    }

    void local_inc_ref(const ID& id) {
        // inc ref can double as an initializer for an IVar.
        db[id].ownership.ref_count++;
    }

    void local_dec_ref(const ID& id) {
        assert(db.contains(id));
        auto& rc = db[id].ownership.ref_count;
        rc--;
        assert(rc >= 0);
        if (rc == 0) {
            erase(id);
        }
    }

    void erase(const ID& id) {
        auto& info = db[id];
        auto& val_loc = info.ownership.val_loc;
        if (val_loc != nullptr) {
            if (!is_local(*val_loc)) {
                comm.send(*val_loc, Msg(Protocol::DeleteInfo, make_data(id)));
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
        db.set_val_loc(iv.id, comm.get_addr());
        fulfill_triggers(iv, db[iv.id].ownership.trigger_locs);
        db[iv.id].ownership.trigger_locs.clear();
    }

    void local_add_trigger(const IVarRef& iv) {
        if (!db.is_fulfilled(iv.id)) {
            db.add_trigger_loc(iv.id, comm.get_addr());
            return;
        }

        auto& info = db[iv.id];
        if (is_local(*info.ownership.val_loc)) {
            db.run_local_triggers(iv.id, info.vals);
        } else {
            comm.send(*info.ownership.val_loc, Msg(Protocol::RecvTriggers, make_data(
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

void IVarTracker::inc_ref(const IVarRef& iv) {
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_inc_ref(iv.id);
    } else {
        impl->comm.send(owner, Msg(Protocol::IncRef, make_data(iv.id)));
    }
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_dec_ref(iv.id);
    } else {
        impl->comm.send(owner, Msg(Protocol::DecRef, make_data(iv.id)));
    }
}

void IVarTracker::introduce(Address addr) {
    impl->ring.introduce(addr);
}

size_t IVarTracker::n_owned() const {
    size_t out = 0;
    for (auto& o: impl->db) {
        if (o.second.ownership.ref_count > 0) {
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

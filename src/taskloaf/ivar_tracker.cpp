#include "ivar_tracker.hpp"
#include "id.hpp"
#include "comm.hpp"
#include "ring.hpp"
#include "protocol.hpp"

#include <unordered_map>
#include <set>
#include <iostream>

namespace taskloaf {

struct IVarOwnership {
    int ref_count = 0;
    //TODO: Also store value size in bytes. For greedy memory transfer.
    std::unique_ptr<Address> val_loc;
    //TODO: Also store trigger sizes in bytes. For greedy memory transfer.
    std::set<Address> trigger_locs;
};

//TODO: Consider breaking this up into Ownership, Vals, and Triggers objects.
struct IVarTrackerImpl {
    Comm& comm;
    Ring ring;
    std::unordered_map<ID,IVarOwnership> ownership;
    std::unordered_map<ID,std::vector<Data>> vals;
    std::unordered_map<ID,std::vector<TriggerT>> triggers;

    IVarTrackerImpl(Comm& comm):
        comm(comm),
        ring(comm, 1)
    {}

    ~IVarTrackerImpl() {
    }

    void set_val_loc(const IVarRef& iv, const Address& loc) {
        assert(ownership[iv.id].val_loc == nullptr);
        ownership[iv.id].val_loc = std::make_unique<Address>(loc);
    }

    void add_trigger_loc(const IVarRef& iv, const Address& loc) {
        ownership[iv.id].trigger_locs.insert(loc);
    }

    bool is_fulfilled(const IVarRef& iv) {
        return ownership[iv.id].val_loc != nullptr;
    }

    void setup_handlers() {
        // On the owner
        comm.add_handler(Protocol::IncRef, [&] (Data d) {
            auto iv = IVarRef(d.get_as<ID>());
            local_inc_ref(iv);
        });
        comm.add_handler(Protocol::DecRef, [&] (Data d) {
            auto iv = IVarRef(d.get_as<ID>());
            local_dec_ref(iv);
        });
        comm.add_handler(Protocol::Fulfill, [&] (Data d) {
            auto p = d.get_as<std::pair<IVarRef,Address>>();
            auto iv = p.first;

            set_val_loc(iv, p.second);

            auto& trigger_locs = ownership[iv.id].trigger_locs;
            comm.send(p.second, Msg(Protocol::TriggerLocs, make_data(
                std::make_pair(std::move(iv), std::move(trigger_locs))
            )));
        });
        comm.add_handler(Protocol::AddTrigger, [&] (Data d) {
            auto p = d.get_as<std::pair<IVarRef,Address>>();
            auto iv = p.first;

            if (is_fulfilled(iv)) {
                assert(ownership[iv.id].trigger_locs.size() == 0);
                auto val_loc = *ownership[iv.id].val_loc;
                comm.send(val_loc, Msg(Protocol::TriggerLocs, make_data(
                    std::make_pair(std::move(iv), std::set<Address>{p.second})
                )));
            } else {
                add_trigger_loc(iv, p.second);
            }
        });

        // On the fulfiller
        comm.add_handler(Protocol::TriggerLocs, [&] (Data d) {
            auto p = d.get_as<std::pair<IVarRef,std::set<Address>>>();
            auto iv = p.first;
            assert(vals.count(iv.id) > 0);
            fulfill_triggers(iv, p.second);
        });
        comm.add_handler(Protocol::RecvTriggers, [&] (Data d) {
            auto p = d.get_as<std::pair<IVarRef,std::vector<TriggerT>>>();
            auto iv = p.first;
            assert(vals.count(iv.id) > 0);
            for (auto& t: p.second) {
                t(vals[iv.id]);
            }
        });
        comm.add_handler(Protocol::DeleteVals, [&] (Data d) {
            auto id = d.get_as<ID>();
            assert(vals.count(id) > 0);
            vals.erase(id);
        });
        
        // On where add_trigger was called.
        comm.add_handler(Protocol::GetTriggers, [&] (Data d) {
            auto p = d.get_as<std::pair<IVarRef,Address>>();
            auto iv = p.first;
            auto& trigs_to_send = triggers[iv.id];
            comm.send(p.second, Msg(Protocol::RecvTriggers, make_data(
                std::make_pair(std::move(iv), std::move(trigs_to_send))
            )));
        });
        comm.add_handler(Protocol::DeleteTriggers, [&] (Data d) {
            auto id = d.get_as<ID>();
            assert(triggers.count(id) > 0);
            triggers.erase(id);
        });
    }

    void local_inc_ref(const IVarRef& iv) {
        // inc ref can double as an initializer for an IVar.
        ownership[iv.id].ref_count++;
    }

    void erase(const ID& id) {
        auto& info = ownership[id];
        if (info.val_loc != nullptr) {
            if (is_local(*info.val_loc)) {
                vals.erase(id);
            } else {
                comm.send(*info.val_loc, Msg(Protocol::DeleteVals, make_data(id)));
            }
        }
        for (auto& t_loc: info.trigger_locs) {
            if (is_local(t_loc)) {
                triggers.erase(id);
            } else {
                comm.send(t_loc, Msg(Protocol::DeleteTriggers, make_data(id)));
            }
        }
        ownership.erase(id);
    }

    void local_dec_ref(const IVarRef& iv) {
        assert(ownership.count(iv.id) > 0);
        auto& rc = ownership[iv.id].ref_count;
        rc--;
        if (rc <= 0) {
            erase(iv.id);
        }
    }

    void local_run_triggers(const IVarRef& iv, std::vector<Data>& input) {
        // We can't just do a for loop over the currently present triggers
        // because a trigger on a given IVar could add another trigger on
        // the same IVar.
        while (triggers[iv.id].size() > 0) {
            auto t = std::move(triggers[iv.id].back());
            triggers[iv.id].pop_back();
            t(input);
        }
        triggers[iv.id].clear();
    }

    void fulfill_triggers(const IVarRef& iv, const std::set<Address>& trigger_locs) {
        for (auto& t_loc: trigger_locs) {
            if (is_local(t_loc)) {
                local_run_triggers(iv, vals[iv.id]);
            } else {
                comm.send(t_loc, Msg(Protocol::GetTriggers, make_data(
                    std::make_pair(iv, comm.get_addr())
                )));
            }
        }
    }

    void local_fulfill(const IVarRef& iv) {
        set_val_loc(iv, comm.get_addr());
        fulfill_triggers(iv, ownership[iv.id].trigger_locs);
        ownership[iv.id].trigger_locs.clear();
    }

    void local_add_trigger(const IVarRef& iv) {
        if (!is_fulfilled(iv)) {
            add_trigger_loc(iv, comm.get_addr());
            return;
        }

        auto& val_loc = *ownership[iv.id].val_loc;
        if (is_local(val_loc)) {
            local_run_triggers(iv, vals[iv.id]);
        } else {
            comm.send(val_loc, Msg(Protocol::RecvTriggers, make_data(
                std::make_pair(iv, std::move(triggers[iv.id]))
            )));
        }
    }

    bool is_local(const Address& addr) {
        return addr == comm.get_addr();
    }
};


IVarTracker::IVarTracker(Comm& comm):
    impl(std::make_unique<IVarTrackerImpl>(comm))
{
    impl->setup_handlers();
}

IVarTracker::IVarTracker(IVarTracker&&) = default;

IVarTracker::~IVarTracker() = default;

std::pair<IVarRef,bool> IVarTracker::new_ivar(const ID& id) {
    return {IVarRef(id), true};
}

void IVarTracker::fulfill(const IVarRef& iv, std::vector<Data> input) {
    assert(input.size() > 0);
    assert(impl->vals.count(iv.id) == 0);
    impl->vals[iv.id] = std::move(input);

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
    impl->triggers[iv.id].push_back(std::move(trigger));
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
        impl->local_inc_ref(iv);
    } else {
        impl->comm.send(owner, Msg(Protocol::IncRef, make_data(iv.id)));
    }
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_dec_ref(iv);
    } else {
        impl->comm.send(owner, Msg(Protocol::DecRef, make_data(iv.id)));
    }
}

void IVarTracker::introduce(Address addr) {
    impl->ring.introduce(addr);
}

size_t IVarTracker::n_owned() const {
    return impl->ownership.size();
}

size_t IVarTracker::n_triggers_here() const {
    return impl->triggers.size();
}

size_t IVarTracker::n_vals_here() const {
    return impl->vals.size();
}

const std::vector<ID>& IVarTracker::get_ring_locs() const {
    return impl->ring.get_locs();
}

size_t IVarTracker::ring_size() const {
    return impl->ring.ring_size();
}

} //end namespace taskloaf

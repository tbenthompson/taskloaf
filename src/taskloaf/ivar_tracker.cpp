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
    int ref_count;
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

    void set_val_loc(const ID& id, const Address& loc) {
        assert(ownership[id].val_loc == nullptr);
        ownership[id].val_loc = std::make_unique<Address>(loc);
    }

    void add_trigger_loc(const ID& id, const Address& loc) {
        ownership[id].trigger_locs.insert(loc);
    }

    bool is_fulfilled(const ID& id) {
        return ownership[id].val_loc != nullptr;
    }

    void setup_handlers() {
        // On the owner
        comm.add_handler(Protocol::IncRef, [&] (Data d) {
            auto id = d.get_as<ID>();
            local_inc_ref(id);
        });
        comm.add_handler(Protocol::DecRef, [&] (Data d) {
            auto id = d.get_as<ID>();
            local_dec_ref(id);
        });
        comm.add_handler(Protocol::NewIVar, [&] (Data d) {
            auto id = d.get_as<ID>();
            local_new_ivar(id);
        });
        comm.add_handler(Protocol::Fulfill, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,Address>>();
            auto id = p.first;

            set_val_loc(id, p.second);

            comm.send(p.second, Msg(Protocol::TriggerLocs, make_data(
                std::make_pair(id, std::move(ownership[id].trigger_locs))
            )));
        });
        comm.add_handler(Protocol::AddTrigger, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,Address>>();
            auto id = p.first;

            if (is_fulfilled(id)) {
                assert(ownership[id].trigger_locs.size() == 0);
                auto val_loc = *ownership[id].val_loc;
                comm.send(val_loc, Msg(Protocol::TriggerLocs, make_data(
                    std::make_pair(id, std::set<Address>{p.second})
                )));
            } else {
                add_trigger_loc(id, p.second);
            }
        });

        // On the fulfiller
        comm.add_handler(Protocol::TriggerLocs, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,std::set<Address>>>();
            auto id = p.first;
            assert(vals.count(id) > 0);
            fulfill_triggers(id, p.second);
        });
        comm.add_handler(Protocol::RecvTriggers, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,std::vector<TriggerT>>>();
            auto id = p.first;
            assert(vals.count(id) > 0);
            for (auto& t: p.second) {
                t(vals[id]);
            }
        });
        
        // On where add_trigger was called.
        comm.add_handler(Protocol::GetTriggers, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,Address>>();
            auto id = p.first;
            comm.send(p.second, Msg(Protocol::RecvTriggers, make_data(
                std::make_pair(id, std::move(triggers[id]))
            )));
        });
    }

    void local_inc_ref(const ID& id) {
        assert(ownership.count(id) > 0);
        ownership[id].ref_count++;
    }

    void local_dec_ref(const ID& id) {
        assert(ownership.count(id) > 0);
        auto& rc = ownership[id].ref_count;
        rc--;
        if (rc <= 0) {
            triggers.erase(id);
            vals.erase(id);
            ownership.erase(id);
        }
    }

    std::pair<IVarRef,bool> local_new_ivar(const ID& id) {
        if (ownership.count(id) == 0) {
            ownership.insert(std::make_pair(id, IVarOwnership{0, nullptr, {}}));
            return {IVarRef(id), true};
        }
        return {IVarRef(id), false};
    }

    void local_run_triggers(const ID& id, std::vector<Data>& input) {
        // We can't just do a for loop over the currently present triggers
        // because a trigger on a given IVar could add another trigger on
        // the same IVar.
        while (triggers[id].size() > 0) {
            auto t = std::move(triggers[id].back());
            triggers[id].pop_back();
            t(input);
        }
        triggers[id].clear();
    }

    void fulfill_triggers(const ID& id, const std::set<Address>& trigger_locs) {
        for (auto& t_loc: trigger_locs) {
            if (is_local(t_loc)) {
                local_run_triggers(id, vals[id]);
            } else {
                comm.send(t_loc, Msg(Protocol::GetTriggers, make_data(
                    std::make_pair(id, comm.get_addr())
                )));
            }
        }
    }

    void local_fulfill(const ID& id) {
        set_val_loc(id, comm.get_addr());
        fulfill_triggers(id, ownership[id].trigger_locs);
        ownership[id].trigger_locs.clear();
    }

    void local_add_trigger(const ID& id) {
        if (!is_fulfilled(id)) {
            add_trigger_loc(id, comm.get_addr());
            return;
        }
        if (is_local(*ownership[id].val_loc)) {
            local_run_triggers(id, vals[id]);
        } else {
            std::cout << "REMOTEFULFILL_LATERTRIGGER" << std::endl;
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
    auto owner = impl->ring.get_owner(id);
    if (impl->is_local(owner)) {
        return impl->local_new_ivar(id);
    } else {
        impl->comm.send(owner, Msg(Protocol::NewIVar, make_data(id)));
        return {IVarRef(id), true};
    }
}

void IVarTracker::fulfill(const IVarRef& iv, std::vector<Data> input) {
    assert(input.size() > 0);
    assert(impl->vals.count(iv.id) == 0);
    impl->vals[iv.id] = std::move(input);

    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_fulfill(iv.id);
    } else {
        impl->comm.send(owner, Msg(Protocol::Fulfill, make_data(
            std::make_pair(iv.id, impl->comm.get_addr())
        )));
    }
}

void IVarTracker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    impl->triggers[iv.id].push_back(std::move(trigger));
    auto owner = impl->ring.get_owner(iv.id);
    if (impl->is_local(owner)) {
        impl->local_add_trigger(iv.id);
    } else {
        impl->comm.send(owner, Msg(Protocol::AddTrigger, make_data(
            std::make_pair(iv.id, impl->comm.get_addr())
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
    return impl->ownership.size();
}

const std::vector<ID>& IVarTracker::get_ring_locs() const {
    return impl->ring.get_locs();
}

} //end namespace taskloaf

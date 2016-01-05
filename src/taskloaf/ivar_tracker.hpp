#pragma once

#include "ivar.hpp"

namespace taskloaf {

struct ID;
struct Data;
struct Comm;

struct IVarTrackerImpl;
struct IVarTracker {
    std::unique_ptr<IVarTrackerImpl> impl;

    IVarTracker(Comm& comm);
    IVarTracker(IVarTracker&&);
    ~IVarTracker();

    std::pair<IVarRef,bool> new_ivar(const ID& id); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);

    void introduce(Address addr);
    size_t n_owned() const;
    const std::vector<ID>& get_ring_locs() const;
};

} //end namespace taskloaf

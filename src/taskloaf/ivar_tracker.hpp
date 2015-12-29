#pragma once

#include "ivar.hpp"

#include <unordered_map>

namespace taskloaf {

struct IVarTracker {
    std::unordered_map<size_t,IVarData> ivars;
    size_t next_ivar_id;

    IVarTracker();

    bool empty();
    IVarRef new_ivar(Address addr); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);
};

} //end namespace taskloaf
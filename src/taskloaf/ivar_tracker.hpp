#pragma once

#include "ivar.hpp"
#include "id.hpp"

#include <unordered_map>

namespace taskloaf {

struct ID;

struct IVarTracker {
    std::unordered_map<ID,int> ref_counts;
    std::unordered_map<ID,std::vector<Data>> vals;
    std::unordered_map<ID,std::vector<TriggerT>> triggers;

    std::pair<IVarRef,bool> new_ivar(Address addr, const ID& id); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);
};

} //end namespace taskloaf

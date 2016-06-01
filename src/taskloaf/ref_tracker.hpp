#pragma once

#include "address.hpp"
#include "closure.hpp"

namespace taskloaf {

struct GlobalRef;

struct RefTracker {
    virtual void introduce(Address addr) = 0;
    virtual void fulfill(const GlobalRef& gref, std::vector<Data> vals) = 0;
    virtual void add_trigger(const GlobalRef& gref, TriggerT trigger) = 0;
    virtual void dec_ref(const GlobalRef& gref) = 0;

    virtual bool is_fulfilled_here(const GlobalRef& gref) const = 0;
    virtual std::vector<Data> get_vals(const GlobalRef& gref) const = 0;
};

} //end namespace taskloaf

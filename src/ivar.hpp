#pragma once

#include "address.hpp"
#include "fnc.hpp"

#include <vector>

namespace taskloaf {

typedef Function<void(std::vector<Data>& val)> TriggerT;

struct IVarData {
    std::vector<Data> vals;
    std::vector<TriggerT> fulfill_triggers;
    size_t ref_count = 0;
};

// A reference counting "pointer" to an IVar
struct IVarRef {
    Address owner;
    size_t id;

    IVarRef(Address owner, size_t id);
    IVarRef(const IVarRef&);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&) = delete;
    IVarRef& operator=(const IVarRef&) = delete;
    ~IVarRef();
};

} //end namespace taskloaf

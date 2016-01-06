#pragma once

#include "address.hpp"
#include "fnc.hpp"
#include "id.hpp"

#include <vector>

namespace taskloaf {

typedef Function<void(std::vector<Data>& val)> TriggerT;

// A reference counting "pointer" to an IVar
struct IVarRef {
    ID id;
    bool moved = false;

    explicit IVarRef(ID id);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&);
    IVarRef(const IVarRef&);
    IVarRef& operator=(const IVarRef&) = delete;
    ~IVarRef();
};

} //end namespace taskloaf

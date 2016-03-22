#pragma once

#include "id.hpp"
#include "ref_counting.hpp"

namespace taskloaf {

// A reference counting "pointer" to an IVar
struct IVarRef {
    ID id;
    RefData data;
    bool empty;

    IVarRef();
    explicit IVarRef(ID id);
    IVarRef(ID id, RefData data);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&);
    IVarRef& operator=(const IVarRef&);
    IVarRef(const IVarRef&);
    ~IVarRef();

    template <typename Archive>
    void serialize(Archive& ar) {
        (void)ar;
    }
};

} //end namespace taskloaf

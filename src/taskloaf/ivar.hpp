#pragma once

#include "id.hpp"
#include "ref_counting.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

// A reference counting "pointer" to an IVar
struct IVarRef {
    ID id;
    mutable RefData data;
    bool empty;

    IVarRef();
    explicit IVarRef(ID id);
    IVarRef(ID id, RefData data);
    IVarRef(const IVarRef&);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&);
    IVarRef& operator=(const IVarRef&);
    ~IVarRef();

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

} //end namespace taskloaf

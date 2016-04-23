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
    IVarRef(const IVarRef&);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&);
    IVarRef& operator=(const IVarRef&);
    ~IVarRef();

    template <typename Archive>
    void save(Archive& ar) const {
        ar(empty);
        if (!empty) {
            ar(id);
            ar(copy_ref(*const_cast<RefData*>(&data)));
        }
    }

    template <typename Archive>
    void load(Archive& ar) {
        ar(empty);
        if (!empty) {
            ar(id);
            ar(data);
        }
    }
};

} //end namespace taskloaf

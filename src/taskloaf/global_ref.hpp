#pragma once

#include "id.hpp"
#include "ref_counting.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

// A reference counting "pointer" to an IVar
struct GlobalRef {
    ID id;
    mutable RefData data;
    bool empty;

    GlobalRef();
    explicit GlobalRef(ID id);
    GlobalRef(ID id, RefData data);
    GlobalRef(const GlobalRef&);
    GlobalRef(GlobalRef&&);
    GlobalRef& operator=(GlobalRef&&);
    GlobalRef& operator=(const GlobalRef&);
    ~GlobalRef();

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

} //end namespace taskloaf

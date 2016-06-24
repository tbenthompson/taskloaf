#pragma once
#include "address.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

enum location {
    anywhere = -2,
    here = -1
};

struct internal_location {
    bool anywhere;
    address where;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

internal_location internal_loc(int loc);

struct closure;
void schedule(const internal_location& iloc, closure t);

} //end namespace taskloaf

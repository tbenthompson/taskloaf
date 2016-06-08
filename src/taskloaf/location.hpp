#pragma once
#include "address.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

enum class Loc: int {
    anywhere = -2,
    here = -1
};

struct InternalLoc {
    bool anywhere;
    Address where;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

InternalLoc internal_loc(int loc);

struct Closure;
void schedule(const InternalLoc& iloc, Closure t);

} //end namespace taskloaf

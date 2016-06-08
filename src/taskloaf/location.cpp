#include "location.hpp"
#include "worker.hpp"

namespace taskloaf {

void InternalLoc::save(cereal::BinaryOutputArchive& ar) const {
    ar(anywhere); ar(where); 
}

void InternalLoc::load(cereal::BinaryInputArchive& ar) {
    ar(anywhere); ar(where); 
}

InternalLoc internal_loc(int loc) {
    if (loc == static_cast<int>(Loc::anywhere)) {
        return {true, {}};
    } else if (loc == static_cast<int>(Loc::here)) {
        return {false, cur_worker->get_addr()};
    } else {
        return {false, {loc}};
    }
}

void schedule(const InternalLoc& iloc, Closure t) {
    if (iloc.anywhere) {
        cur_worker->add_task(std::move(t));
    } else {
        cur_worker->add_task(iloc.where, std::move(t));
    }
}

} //end namespace taskloaf

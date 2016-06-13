#include "location.hpp"
#include "worker.hpp"

namespace taskloaf {

void internal_location::save(cereal::BinaryOutputArchive& ar) const {
    ar(anywhere); ar(where); 
}

void internal_location::load(cereal::BinaryInputArchive& ar) {
    ar(anywhere); ar(where); 
}

internal_location internal_loc(int loc) {
    if (loc == static_cast<int>(location::anywhere)) {
        return {true, {}};
    } else if (loc == static_cast<int>(location::here)) {
        return {false, cur_worker->get_addr()};
    } else {
        return {false, {loc}};
    }
}

void schedule(const internal_location& iloc, closure t) {
    if (iloc.anywhere) {
        cur_worker->add_task(std::move(t));
    } else {
        cur_worker->add_task(iloc.where, std::move(t));
    }
}

} //end namespace taskloaf

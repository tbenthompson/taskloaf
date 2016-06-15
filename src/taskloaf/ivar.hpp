#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"

namespace taskloaf {

struct ivar_data {
    data val;
    closure triggers;
    address owner = cur_addr;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

struct ivar {
    data iv;

    ivar();

    bool fulfilled() const;
    void add_trigger(closure trigger);
    void fulfill(data vals);
    data& get();
};

} //end namespace taskloaf

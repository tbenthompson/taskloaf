#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"

namespace taskloaf {

struct ivar_data {
    data_ptr val;
    std::vector<closure> triggers;
    address owner = cur_addr;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

struct ivar {
    std::shared_ptr<ivar_data> iv;

    ivar();

    void add_trigger(closure trigger);
    void fulfill(data_ptr vals);
    data_ptr& get();
};

} //end namespace taskloaf

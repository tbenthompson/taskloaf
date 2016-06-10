#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"

namespace taskloaf {

struct IVarData {
    Data val;
    std::vector<closure> triggers;
    Address owner = cur_addr;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

struct IVar {
    std::shared_ptr<IVarData> data;

    IVar();

    void add_trigger(closure trigger);
    void fulfill(Data vals);
    Data get();
};

} //end namespace taskloaf

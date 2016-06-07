#pragma once
#include "closure.hpp"
#include "address.hpp"

namespace taskloaf {

using TriggerT = Closure<void(std::vector<Data>&)>;
using TaskT = Closure<void()>;

struct IVarData {
    bool fulfilled = false;
    std::vector<Data> vals;
    std::vector<TriggerT> triggers;
    Address owner;

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

    void add_trigger(TriggerT trigger);
    void fulfill(std::vector<Data> vals);
    std::vector<Data> get_vals();
};

} //end namespace taskloaf

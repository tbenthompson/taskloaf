#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"
#include "remote_ref.hpp"

namespace taskloaf {

struct ivar_data {
    data val;
    closure triggers;
    size_t refs;
};

using ivar_data_ref = remote_ref<ivar_data>;

struct ivar {
    remote_ref<ivar_data> rr;

    bool fulfilled_here() const;
    void add_trigger(closure trigger);
    void fulfill(data vals);
    data get();
};

} //end namespace taskloaf

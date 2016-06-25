#pragma once
#include "address.hpp"
#include "ivar.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

struct untyped_future {
    ivar internal;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    untyped_future then(closure fnc);
    untyped_future then(address where, closure fnc);

    untyped_future unwrap();

    data get();
    bool fulfilled_here() const;
};

untyped_future ut_ready(data d);
untyped_future ut_task(address where, closure fnc);
untyped_future ut_task(closure fnc);

} //end namespace taskloaf

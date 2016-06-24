#pragma once
#include "location.hpp"
#include "ivar.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

struct untyped_future {
    ivar internal;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    untyped_future then(closure fnc);
    untyped_future then(int loc, closure fnc);

    untyped_future unwrap();

    data get();
    bool fulfilled_here() const;
};

untyped_future ut_ready(data d);

template <typename T>
untyped_future ut_ready(T&& v) { return ut_ready(data(std::forward<T>(v))); }

untyped_future ut_task(int loc, closure fnc);
untyped_future ut_task(closure fnc);

} //end namespace taskloaf

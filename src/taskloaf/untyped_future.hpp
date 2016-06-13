#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

struct untyped_future;

untyped_future ready(data_ptr d);
untyped_future then(int loc, untyped_future& f, closure fnc);
untyped_future async(int loc, closure fnc);
untyped_future async(location loc, closure fnc);
untyped_future async(closure fnc);
untyped_future unwrap(untyped_future& fut);

struct untyped_future {
    ivar v;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    untyped_future then(closure f);
    untyped_future then(location loc, closure f);
    untyped_future then(int loc, closure f);
    untyped_future unwrap();
    data_ptr& get();
    void wait();
};

untyped_future when_all(std::vector<untyped_future> fs);

} //end namespace taskloaf

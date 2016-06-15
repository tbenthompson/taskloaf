#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

struct future;


future ready(data d);

future then(int loc, future& f, closure fnc);
future async(int loc, closure fnc);
future async(location loc, closure fnc);
future async(closure fnc);
future unwrap(future& fut);

struct future {
    ivar internal;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    future then(closure f);
    future then(location loc, closure f);
    future then(int loc, closure f);
    future unwrap();
    bool fulfilled() const;
    data& get();
    void wait();
};

template <typename T>
future ready(T&& v) { return ready(data(std::forward<T>(v))); }

future when_all(std::vector<future> fs);

} //end namespace taskloaf

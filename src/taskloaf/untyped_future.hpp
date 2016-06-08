#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

struct UntypedFuture;

UntypedFuture ready(Data d);
UntypedFuture then(int loc, UntypedFuture& f, Closure fnc);
UntypedFuture async(int loc, Closure fnc);
UntypedFuture async(Loc loc, Closure fnc);
UntypedFuture async(Closure fnc);
UntypedFuture unwrap(UntypedFuture& fut);

struct UntypedFuture {
    IVar ivar;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    UntypedFuture then(Closure f);
    UntypedFuture then(Loc loc, Closure f);
    UntypedFuture then(int loc, Closure f);
    UntypedFuture unwrap();
    Data get();
    void wait();
};

UntypedFuture when_all(std::vector<UntypedFuture> fs);

} //end namespace taskloaf

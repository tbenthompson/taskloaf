#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

struct UntypedFuture;

UntypedFuture ready(Data d);
UntypedFuture then(int loc, UntypedFuture& f, closure fnc);
UntypedFuture async(int loc, closure fnc);
UntypedFuture async(Loc loc, closure fnc);
UntypedFuture async(closure fnc);
UntypedFuture unwrap(UntypedFuture& fut);

struct UntypedFuture {
    IVar ivar;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    UntypedFuture then(closure f);
    UntypedFuture then(Loc loc, closure f);
    UntypedFuture then(int loc, closure f);
    UntypedFuture unwrap();
    Data get();
    void wait();
};

UntypedFuture when_all(std::vector<UntypedFuture> fs);

} //end namespace taskloaf

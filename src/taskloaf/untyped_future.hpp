#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

using ThenTaskT = Closure<std::vector<Data>(std::vector<Data>&)>;
using AsyncTaskT = Closure<std::vector<Data>()>;

struct UntypedFuture;

UntypedFuture ready(std::vector<Data> d);
UntypedFuture then(int loc, UntypedFuture& f, ThenTaskT fnc);
UntypedFuture async(int loc, AsyncTaskT fnc);
UntypedFuture async(Loc loc, AsyncTaskT fnc);
UntypedFuture async(AsyncTaskT fnc);
UntypedFuture unwrap(UntypedFuture& fut);

struct UntypedFuture {
    IVar ivar;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    UntypedFuture then(ThenTaskT f);
    UntypedFuture then(Loc loc, ThenTaskT f);
    UntypedFuture then(int loc, ThenTaskT f);
    UntypedFuture unwrap();
    std::vector<Data> get();
    void wait();
};

UntypedFuture when_all(std::vector<UntypedFuture> fs);

} //end namespace taskloaf

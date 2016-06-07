#pragma once

#include "closure.hpp"
#include "location.hpp"
#include "worker.hpp"

#include <cereal/types/memory.hpp>

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

    void add_trigger(TriggerT trigger);
    void fulfill(std::vector<Data> vals);

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    UntypedFuture then(ThenTaskT f);
    UntypedFuture then(Loc loc, ThenTaskT f);
    UntypedFuture then(int loc, ThenTaskT f);
    UntypedFuture unwrap();
    std::vector<Data> get();
    void wait();
};

} //end namespace taskloaf

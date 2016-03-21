#pragma once

#include <cassert>

#include "ivar.hpp"
#include "fnc.hpp"

namespace taskloaf {

typedef Function<Data(std::vector<Data>&)> PureTaskT;

template <typename... Ts> struct Future;

IVarRef plan_then(const IVarRef& input, PureTaskT task);
IVarRef plan_unwrap(const IVarRef& input, PureTaskT unwrapper);
IVarRef plan_ready(Data data);
IVarRef plan_async(PureTaskT task);
IVarRef plan_when_all(std::vector<IVarRef> inputs);

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F fnc) {
    typedef std::result_of_t<F(Ts&...)> Return;
    std::function<Return(Ts&...)> container_fnc(std::move(fnc));
    auto task =
        [container_fnc = std::move(container_fnc)] (std::vector<Data>& in) mutable
        {
            return make_data(apply_args(in, container_fnc));
        };
    return Future<Return>{plan_then(fut.ivar, std::move(task))};
}

template <typename T>
auto unwrap(const Future<T>& fut) {
    auto unwrapper = [] (std::vector<Data>& in) {
        return make_data(in[0].get_as<T>().ivar);
    };
    return Future<typename T::type>{plan_unwrap(fut.ivar, std::move(unwrapper))};
}

template <typename T>
auto ready(T val) {
    return Future<T>{plan_ready(make_data(std::move(val)))};
}

template <typename F>
auto async(F fnc) {
    auto task = [fnc = std::move(fnc)] (std::vector<Data>&) mutable {
        return make_data(fnc());
    };
    return Future<std::result_of_t<F()>>{plan_async(std::move(task))};
}

template <typename... Ts>
auto when_all(const Future<Ts>&... args) {
    std::vector<IVarRef> data{args.ivar...};
    return Future<Ts...>{plan_when_all(std::move(data))};
}

template <typename... Ts>
struct Future {
    IVarRef ivar;

    template <typename F>
    auto then(F f) const {
        return taskloaf::then(*this, f);
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    IVarRef ivar;

    template <typename F>
    auto then(F f) const {
        return taskloaf::then(*this, f);
    }
    
    auto unwrap() const {
        return taskloaf::unwrap(*this);
    }
};

void launch_helper(size_t n_workers, std::function<IVarRef()> f);
int shutdown();

template <typename F>
void launch(int n_workers, F f) {
    launch_helper(n_workers, [f = std::move(f)] () {
        auto t = async(f).unwrap();
        return t.ivar;
    });
}

} //end namespace taskloaf

#pragma once

#include <cassert>

#include "ivar.hpp"
#include "fnc.hpp"
#include "execute.hpp"
#include "closure.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

IVarRef plan_then(const IVarRef& input, PureTaskT task);
IVarRef plan_unwrap(const IVarRef& input, PureTaskT unwrapper);
IVarRef plan_ready(Data data);
IVarRef plan_async(PureTaskT task);
IVarRef plan_when_all(std::vector<IVarRef> inputs);

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F&& fnc) {
    typedef std::result_of_t<F(Ts&...)> Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    Closure<Data(std::vector<Data>&)> task(
        [] (std::vector<Data>& d, std::vector<Data>& in) mutable
        {
            return make_data(apply_args(in, d[0].get_as<decltype(fnc_container)>()));
        },
        {make_data(std::move(fnc_container))}
    );
    return Future<Return>{plan_then(fut.ivar, std::move(task))};
}

template <typename T>
auto unwrap(const Future<T>& fut) {
    Closure<Data(std::vector<Data>&)> task(
        [] (std::vector<Data>&, std::vector<Data>& in) {
            return make_data(in[0].get_as<T>().ivar);
        },
        {}
    );
    return Future<typename T::type>{plan_unwrap(fut.ivar, std::move(task))};
}

template <typename T>
auto ready(T val) {
    return Future<T>{plan_ready(make_data(std::move(val)))};
}

template <typename F>
auto async(F&& fnc) {
    typedef std::result_of_t<F()> Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    Closure<Data(std::vector<Data>&)> task(
        [] (std::vector<Data>& d, std::vector<Data>&) {
            return make_data(d[0].get_as<decltype(fnc_container)>()());
        },
        {make_data(std::move(fnc_container))}
    );
    return Future<Return>{plan_async(std::move(task))};
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

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ivar);
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

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ivar);
    }
};

void launch_helper(size_t n_workers, std::function<IVarRef()> f);
int shutdown();

template <typename F>
void launch(int n_workers, F f) {
    launch_helper(n_workers, [f = std::move(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });
}

} //end namespace taskloaf

#pragma once
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoidCall {};

template <typename Return, typename... Args>
struct PossiblyVoidCall<Return(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        return std::make_tuple(f());
    }
};

template <typename... Args>
struct PossiblyVoidCall<void(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        f();
        return std::make_tuple();
    }
};

template <typename F>
auto possibly_void_call(const F& f) {
    return PossiblyVoidCall<GetSignature<F>>::on(f);
}

template <typename F, typename... Ts>
auto then(Future<Ts...>& fut, F fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    Future<Return> out_future;

    auto trigger_fnc = [] (F& fnc, Future<Return>& out_future, std::tuple<Ts...>& val) {
        auto task_fnc = [] (F& fnc, Future<Return>& out, std::tuple<Ts...>& val) {
            out.fulfill(possibly_void_call([&] () {
                return apply_args(fnc, val);
            }));
        };
        add_task(
            task_fnc,
            std::move(fnc),
            out_future,
            val
        );
    };
    fut.add_trigger(
        trigger_fnc,
        std::move(fnc),
        out_future
    );
    return out_future;
}

template <typename T>
auto unwrap(Future<T>& fut) {
    typedef Future<typename T::type> FutT;
    FutT out_future;
    fut.add_trigger(
        [] (FutT& out_future, std::tuple<T>& result) {
            std::get<0>(result).add_trigger(
                [] (FutT& out_future, std::tuple<typename T::type> val) {
                    out_future.fulfill(std::move(val)); 
                },
                out_future
            );
        },
        out_future
    );
    return out_future;
}

template <typename T>
auto ready(T val) {
    Future<T> out_future;
    out_future.fulfill(std::make_tuple(std::move(val)));
    return out_future;
}

template <typename F>
auto async(F fnc) {
    typedef typename std::result_of<F()>::type Return;
    Future<Return> out_future;
    using OutT = decltype(out_future);
    add_task(
        [] (OutT& fut, F& f) {
            auto out = possibly_void_call(f);
            fut.fulfill(out);
        },
        out_future,
        std::move(fnc)
    );
    return out_future;
}

} //end namespace taskloaf

#pragma once

#include "closure.hpp"
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoidCall {};

template <typename Return, typename... Args>
struct PossiblyVoidCall<Return(Args...)> {
    template <typename F>
    static auto on(F&& f) {
        return std::make_tuple(f());
    }
};

template <typename... Args>
struct PossiblyVoidCall<void(Args...)> {
    template <typename F>
    static auto on(F&& f) {
        f();
        return std::make_tuple();
    }
};

template <typename... Ts>
auto make_future();

template <typename F, typename... Ts>
auto then(Future<Ts...>& fut, F fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    typedef decltype(make_future<Return>()) OutT;
    OutT out_future = make_future<Return>();
    fut.add_trigger(
        [] (F& fnc, OutT& out_future, std::tuple<Ts...>& val) {
            cur_worker->add_task(
                [] (F& fnc, OutT& out, std::tuple<Ts...>& val) {
                    out.fulfill(PossiblyVoidCall<get_signature<F>>::on([&] () {
                        return apply_args(fnc, val);         
                    }));
                },
                std::move(fnc),
                out_future, //TODO: It should be possible to avoid this copy and the equivalent ones in unwrap and when_all
                val
            );
        },
        std::move(fnc),
        out_future
    );
    return out_future;
}

template <typename T>
auto unwrap(Future<T>& fut) {
    Future<typename T::type> out_future = make_future<typename T::type>();
    typedef decltype(out_future) OutT;
    fut.add_trigger(
        [] (OutT& out_future, std::tuple<T>& result) {
            std::get<0>(result).add_trigger(
                [] (OutT& out_future, std::tuple<typename T::type> val) {
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
    auto out_future = make_future<T>();
    out_future.fulfill(std::make_tuple(std::move(val)));
    return out_future;
}

template <typename F>
auto async(F fnc) {
    typedef typename std::result_of<F()>::type Return;
    auto out_future = make_future<Return>();
    using OutT = decltype(out_future);
    cur_worker->add_task(
        [] (OutT& fut, F& f) {
            auto out = PossiblyVoidCall<get_signature<F>>::on(f);
            fut.fulfill(out);
        },
        out_future,
        std::move(fnc)
    );
    return out_future;
}

template <size_t Idx, bool end, typename... Ts>
struct WhenAllHelper {
    static void run(std::tuple<Future<Ts>...>& child_results, 
        std::tuple<Ts...>& accum, Future<Ts...>& result) 
    {
        typedef std::tuple<
            typename std::tuple_element<Idx,std::tuple<Ts...>>::type
        > ValT;

        auto next_result = std::get<Idx>(child_results);
        next_result.add_trigger(
            [] (std::tuple<Future<Ts>...>& child_results,
                std::tuple<Ts...>& accum, Future<Ts...>& result, 
                ValT& val) 
            {
                std::get<Idx>(accum) = std::get<0>(val);
                WhenAllHelper<Idx+1,Idx+2 == sizeof...(Ts),Ts...>::run(
                    child_results, accum, result
                );
            },
            child_results, accum, result
        );
    }
};

template <size_t Idx, typename... Ts>
struct WhenAllHelper<Idx, true, Ts...> {
    static void run(std::tuple<Future<Ts>...>& child_results, 
        std::tuple<Ts...>& accum, Future<Ts...>& result) 
    {
        typedef std::tuple<
            typename std::tuple_element<Idx,std::tuple<Ts...>>::type
        > ValT;

        std::get<Idx>(child_results).add_trigger(
            [] (Future<Ts...>& result, std::tuple<Ts...>& accum, ValT& val) 
            {
                std::get<Idx>(accum) = std::get<0>(val);
                result.fulfill(std::move(accum));
            },
            result, accum
        );
    }
};

template <typename... FutureTs>
auto when_all(FutureTs&&... args) {
    auto data = std::make_tuple(std::forward<FutureTs>(args)...);
    auto out_future = make_future<
        typename std::decay<FutureTs>::type::type...
    >();
    std::tuple<typename std::decay<FutureTs>::type::type...> accum;
    WhenAllHelper<
        0, sizeof...(FutureTs) == 1,
        typename std::decay<FutureTs>::type::type...
    >::run(
        data, accum, out_future
    );
    return out_future;
}

} //end namespace taskloaf

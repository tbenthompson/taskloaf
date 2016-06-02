#pragma once
#include "worker.hpp"
#include "data.hpp"
#include "closure.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

struct InternalLoc {
    bool anywhere;
    Address where;

    template <typename Ar> 
    void serialize(Ar& ar) { ar(anywhere); ar(where); }
};

InternalLoc internal_loc(int loc) {
    if (loc == static_cast<int>(Loc::anywhere)) {
        return {true, {}};
    } else if (loc == static_cast<int>(Loc::here)) {
        return {false, cur_worker->get_addr()};
    } else {
        return {false, {loc}};
    }
}

void schedule(const InternalLoc& iloc, Closure<void()> t) {
    auto& task_collection = cur_worker->get_task_collection();
    if (iloc.anywhere) {
        task_collection.add_task(std::move(t));
    } else {
        task_collection.add_task(iloc.where, std::move(t));
    }
}

template <typename Return>
struct RemoveTupleFuture {
    using type = Future<Return>;
};

template <typename... Ts>
struct RemoveTupleFuture<std::tuple<Ts...>> {
    using type = Future<Ts...>;
};

template <typename F, typename... Ts>
using OutFutureT = typename RemoveTupleFuture<std::result_of_t<F(Ts&...)>>::type;

template <typename F, typename... Ts>
auto then(int loc, Future<Ts...>& fut, F&& fnc) {
    typedef OutFutureT<F,Ts...> OutFut;

    OutFut out_future;
    auto f_serializable = make_function(std::forward<F>(fnc));
    typedef decltype(f_serializable) FType;
    auto iloc = internal_loc(loc);
    fut.add_trigger(TriggerT(
        [] (FType& f, OutFut& out_future,
            InternalLoc iloc, std::vector<Data>& args) 
        {
            Closure<void()> t(
                [] (FType& f, OutFut& out_future, std::vector<Data>& args)  {
                    out_future.template fulfill_with<FType&&,Ts...>(std::move(f), args);
                },
                std::move(f),
                std::move(out_future),
                args
            );
            schedule(iloc, std::move(t));
        },
        std::move(f_serializable),
        out_future,
        iloc
    ));
    return out_future;
}

template <typename Fut>
auto unwrap(Fut&& fut) {
    typedef typename std::decay_t<Fut>::type T;
    typedef Future<typename T::type> FutT;

    FutT out_future;
    fut.add_trigger(TriggerT(
        [] (FutT& out_future, std::vector<Data>& args) {
            T& inner_fut = args[0].get_as<T>();
            inner_fut.add_trigger(TriggerT(
                [] (FutT& out_future, std::vector<Data>& args) {
                    out_future.fulfill_helper(args);
                },
                std::move(out_future)
            ));
        },
        out_future
    ));
    return out_future;
}

template <typename T>
auto ready(T&& val) {
    Future<std::decay_t<T>> out_future;
    out_future.fulfill(std::forward<T>(val));
    return out_future;
}

template <typename F>
auto async(int loc, F&& fnc) {
    typedef OutFutureT<F> OutFut;
    OutFut out_future;

    auto f_serializable = make_function(fnc);
    typedef decltype(f_serializable) FType;
    auto iloc = internal_loc(loc);
    Closure<void()> t(
        [] (FType& f, OutFut& out_future) {
            out_future.fulfill_with(std::move(f));
        },
        f_serializable, out_future
    );
    schedule(iloc, std::move(t));
    return out_future;
}

template <typename F>
auto async(Loc loc, F&& fnc) {
    return async(static_cast<int>(loc), std::forward<F>(fnc));
}

template <typename F>
auto async(F&& fnc) {
    return async(Loc::anywhere, std::forward<F>(fnc));
}

} //end namespace taskloaf

#pragma once
#include "worker.hpp"
#include "data.hpp"
#include "closure.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename T>
struct AlwaysData {
    using type = Data;
};
template <typename T>
using AlwaysDataT = typename AlwaysData<T>::type;

template <typename... Ts> struct Future;

enum class Loc: int {
    anywhere = -2,
    here = -1
};

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
    if (iloc.anywhere) {
        cur_worker->add_task(std::move(t));
    } else {
        cur_worker->add_task(iloc.where, std::move(t));
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
    typedef OutFutureT<F,AlwaysDataT<Ts>...> OutFut;

    OutFut out_future;
    auto f_serializable = make_function(std::forward<F>(fnc));
    typedef decltype(f_serializable) FType;
    auto iloc = internal_loc(loc);
    fut.add_trigger(typename Future<Ts...>::TriggerT(
        [] (FType& f, OutFut& out_future,
            InternalLoc iloc, std::tuple<AlwaysDataT<Ts>...>& args) 
        {
            Closure<void()> t(
                [] (FType& f, OutFut& out_future, std::tuple<AlwaysDataT<Ts>...>& args)  {
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
    typedef std::decay_t<Fut> DecayF;
    typedef typename DecayF::type T;

    T out_future;
    fut.add_trigger(typename DecayF::TriggerT(
        [] (T& out_future, typename DecayF::DataTupleT& args) {
            T& inner_fut = std::get<0>(args);
            inner_fut.add_trigger(typename T::TriggerT(
                [] (T& out_future, typename T::DataTupleT& args) {
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

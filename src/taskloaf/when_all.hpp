#pragma once

namespace taskloaf {

template <typename T1, typename T2>
struct WhenBothOutT {};

// This flattens two tuples into a Future so that tuples don't
// get nested.
template <typename... T1s, typename... T2s>
struct WhenBothOutT<std::tuple<T1s...>, std::tuple<T2s...>> {
    using type = Future<T1s...,T2s...>;
};

template <typename F1, typename F2, typename... T1s, typename... T2s>
auto when_both_helper(F1&& fut1, F2&& fut2, std::tuple<T1s...>, std::tuple<T2s...>) {
    return fut1.then(Closure<Future<T1s...,T2s...>(Data<T1s>&...)>(
        [] (F2& fut2, Data<T1s>&... args1) {
            return fut2.then(Closure<std::tuple<T1s...,T2s...>(Data<T2s>&...)>(
                [] (Data<T1s>&... args1, Data<T2s>&... args2) {
                    return std::make_tuple(args1..., args2...);
                },
                args1...
            ));
        }, 
        std::forward<F2>(fut2)
    )).unwrap();
}

template <typename F1, typename F2>
auto when_both(F1&& fut1, F2&& fut2) {
    return when_both_helper(
        std::forward<F1>(fut1), std::forward<F2>(fut2),
        typename std::decay_t<F1>::TupleT{}, typename std::decay_t<F2>::TupleT{}
    );
}

// The base case of waiting until one Future has completed (simply
// return the Future!)
template <typename FutureT>
FutureT&& when_all(FutureT&& fut) {
    return std::forward<FutureT>(fut);
}

template <typename F1, typename F2>
auto when_all(F1&& tup1, F2&& tup2)
{
    // The base case of waiting until two Futures have completed.
    return when_both(
        std::forward<F1>(tup1),
        std::forward<F2>(tup2)
    );
}

template <typename Head1Future, typename Head2Future, typename... TailFutures>
auto when_all(Head1Future&& fut1, Head2Future&& fut2, TailFutures&&... tail)
{
    // Build the general case by recursively calling the two Future base case.
    return when_all(
        when_all(
            std::forward<Head1Future>(fut1),
            std::forward<Head2Future>(fut2)
        ),
        when_all(
            std::forward<TailFutures>(tail)...
        )
    );
}

} //end namespace taskloaf 

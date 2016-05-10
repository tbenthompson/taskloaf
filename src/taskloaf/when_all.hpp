#pragma once

namespace taskloaf {

template <typename T1, typename T2>
struct WhenBothOutT {};

template <typename... T1s, typename... T2s>
struct WhenBothOutT<std::tuple<T1s...>, std::tuple<T2s...>> {
    using type = Future<T1s...,T2s...>;
};

template <typename Future1, size_t... Idx1, typename Future2, size_t... Idx2>
auto when_all(Future1&& fut1, Future2&& fut2,
    std::index_sequence<Idx1...>, std::index_sequence<Idx2...>)
{
    typedef typename std::decay<Future1>::type DecayF1;
    typedef typename std::decay<Future2>::type DecayF2;
    typedef typename DecayF1::TupleT In1;
    typedef typename DecayF2::TupleT In2;
    typedef typename WhenBothOutT<In1,In2>::type OutF;
    OutF out_future;

    fut1.add_trigger(
        [] (OutF& out_future, Future2& fut2, In1& r1) {
            fut2.add_trigger(
                [] (OutF& out_future, In1& r1, In2& r2) {
                    out_future.fulfill(std::tuple_cat(r1, r2)); 
                },
                out_future,
                r1
            );
        },
        out_future, fut2
    );
    return out_future;
}

template <typename FutureT>
FutureT&& when_all(FutureT&& fut) {
    return std::forward<FutureT>(fut);
}

template <typename Future1, typename Future2>
auto when_all(Future1&& tup1, Future2&& tup2)
{
    return when_all(
        std::forward<Future1>(tup1),
        std::forward<Future2>(tup2),
        std::make_index_sequence<
            std::tuple_size<typename std::decay<Future1>::type::TupleT>::value
        >{},
        std::make_index_sequence<
            std::tuple_size<typename std::decay<Future2>::type::TupleT>::value
        >{}
    );
}

template <typename Head1Future, typename Head2Future, typename... TailFutures>
auto when_all(Head1Future&& fut1, Head2Future&& fut2, TailFutures&&... tail)
{
    return when_all(
        when_all(
            std::forward<Head1Future>(fut1),
            std::forward<Head2Future>(fut2)
        ),
        std::forward<TailFutures>(tail)...
    );
}

} //end namespace taskloaf 

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

// template <typename Future1, typename Future2, typename... T1s, typename... T2s>
// auto when_both_helper(Future1&& fut1, Future2&& fut2) {
//     return fut1.then(Closure(
//         //TODO: Need typed data.
//         [] (Future2& fut2, Data<T1s>&... args1) {
//             return fut2.then(Closure(
//                 [] (Data<T1s>&... args1, Data<T2s>&... args2) {
//                     //TODO: Need to be able to return multiple arguments
//                     return {args1..., args2...};
//                 },
//                 args1...
//             );
//         }, 
//         std::forward<Future2>(fut2)
//     }).unwrap();
// }

template <typename Future1, typename Future2>
auto when_both(Future1&& fut1, Future2&& fut2)
{
    typedef std::decay_t<Future1> DecayF1;
    typedef std::decay_t<Future2> DecayF2;
    typedef typename DecayF1::TupleT In1;
    typedef typename DecayF2::TupleT In2;
    typedef typename WhenBothOutT<In1,In2>::type OutF;

    OutF out_future;
    fut1.add_trigger(TriggerT(
        [] (OutF& out_future, DecayF2& fut2, std::vector<Data>& args) {
            fut2.add_trigger(TriggerT(
                [] (OutF& out_future, std::vector<Data>& args1,
                    std::vector<Data>& args2) 
                {
                    args1.insert(args1.end(), args2.begin(), args2.end());
                    out_future.fulfill_helper(args1);
                },
                std::move(out_future),
                std::move(args)
            ));
        },
        out_future, 
        std::forward<Future2>(fut2)
    ));
    return out_future;

}

// The base case of waiting until one Future has completed (simply
// return the Future!)
template <typename FutureT>
FutureT&& when_all(FutureT&& fut) {
    return std::forward<FutureT>(fut);
}

template <typename Future1, typename Future2>
auto when_all(Future1&& tup1, Future2&& tup2)
{
    // The base case of waiting until two Futures have completed.
    return when_both(
        std::forward<Future1>(tup1),
        std::forward<Future2>(tup2)
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

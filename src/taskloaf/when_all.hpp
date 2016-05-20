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

template <typename Future1, typename Future2>
auto when_both(Future1&& fut1, Future2&& fut2)
{
    typedef typename std::decay<Future1>::type DecayF1;
    typedef typename std::decay<Future2>::type DecayF2;
    typedef typename DecayF1::TupleT In1;
    typedef typename DecayF2::TupleT In2;
    typedef typename WhenBothOutT<In1,In2>::type OutF;


    bool immediately = fut1.can_trigger_immediately() && fut2.can_trigger_immediately();
    if (immediately) {
        return OutF(fut1.owner, std::tuple_cat(fut1.get(), fut2.get()));
    } else {
        OutF out_future;
        fut1.add_trigger(TriggerT{
            [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                c_args[1].get_as<DecayF2>().add_trigger(TriggerT{
                    [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                        c_args[0].get_as<OutF>().fulfill(std::tuple_cat(
                            c_args[1].get_as<In1>(), args[0].get_as<In2>()
                        ));
                    },
                    {c_args[0], args[0]}
                });
            },
            { make_data(out_future), make_data(fut2) }
        });
        return out_future;
    }
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

#pragma once

#include <tuple>

#include "data.hpp"
#include "get_signature.hpp"

namespace taskloaf {

template <size_t I, typename TupleType>
auto& extract_data(std::vector<Data>& args) {
    typedef typename std::tuple_element<I,TupleType>::type T;
    tlassert(I < args.size());
    return args[I].get_as<typename std::decay<T>::type>();
}

template <typename Func, typename Args, typename IndexList>
struct Applier;

template <typename Func, typename Tuple, size_t... I>
struct Applier<Func, Tuple, std::index_sequence<I...>> {
    static auto on(Func&& func, std::vector<Data>& args) {
        return func(extract_data<I,Tuple>(args)...);
    }
};

template <typename T>
struct ApplyArgsHelper {};

template <typename Return, typename... Args>
struct ApplyArgsHelper<Return(Args...)>
{
    template <typename F>
    static Return run(F&& f, std::vector<Data>& args) {
        return Applier<
            F,std::tuple<Args...>,
            std::make_index_sequence<sizeof...(Args)>
        >::on(std::forward<F>(f), args);
    }
};

template <typename F>
auto apply_data_args(F&& f, std::vector<Data>& args) {
    typedef typename std::decay<F>::type DecayF;
    return ApplyArgsHelper<GetSignature<DecayF>>::run(std::forward<F>(f), args);
}

template <typename IndexList>
struct apply_args_helper;

template <size_t... I>
struct apply_args_helper<std::index_sequence<I...>> {
    template <typename F, typename Tuple>
    static auto apply(F&& func, Tuple&& args) {
        return std::forward<F>(func)(
            std::get<I>(std::forward<Tuple>(args))...
        );
    }
};

template <typename F, typename Tuple>
auto apply_args(F&& f, Tuple&& t) {
    using helper = apply_args_helper<
        std::make_index_sequence<std::tuple_size<
            typename std::decay<Tuple>::type>::value
        >
    >;
    return helper::apply(std::forward<F>(f), std::forward<Tuple>(t));
}

} //end namespace taskloaf

#pragma once

#include <tuple>

#include "data.hpp"
#include "get_signature.hpp"

namespace taskloaf {

template <typename Func, size_t... I>
static auto apply_data_args_helper(Func&& func,
    std::vector<Data>& args, std::index_sequence<I...>) 
{
    return func(args[I]...);
}

template <typename T>
struct ApplyArgsHelper {};

template <typename Return, typename... Args>
struct ApplyArgsHelper<Return(Args...)>
{
    template <typename F>
    static Return run(F&& f, std::vector<Data>& args) {
        return apply_data_args_helper(
            std::forward<F>(f), args, std::index_sequence_for<Args...>{}
        );
    }
};

template <typename F>
auto apply_data_args(F&& f, std::vector<Data>& args) {
    typedef GetSignature<std::decay_t<F>> FSignature;
    return ApplyArgsHelper<FSignature>::run(std::forward<F>(f), args);
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

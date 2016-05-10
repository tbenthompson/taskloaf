#pragma once

#include <tuple>

#include "data.hpp"

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

template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{
    template <typename F>
    static ReturnType run(F&& f, std::vector<Data>& args) {
        return Applier<
            F,std::tuple<Args...>,
            std::make_index_sequence<sizeof...(Args)>
        >::on(std::forward<F>(f), args);
    }
};

// Overload for const member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...) const>: 
    public ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{};

template <typename F>
auto apply_data_args(F&& f, std::vector<Data>& args) {
    typedef typename std::decay<F>::type DecayF;
    return ApplyArgsHelper<
        decltype(&DecayF::operator())
    >::run(std::forward<F>(f), args);
}

template <typename F, typename... Args, std::size_t... I>
auto 
apply_args_helper(const F& f, std::tuple<Args...>& args, std::index_sequence<I...>) {
    return f(std::get<I>(args)...);
}

template <typename F, typename... Args>
auto apply_args(const F& f, std::tuple<Args...>& args) {
    return apply_args_helper(f, args, std::index_sequence_for<Args...>{});
}

} //end namespace taskloaf

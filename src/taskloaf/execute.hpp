#pragma once

#include <tuple>

#include "data.hpp"

namespace taskloaf {

template <size_t I, typename TupleType>
auto& extract_data(std::vector<Data>& args) {
    typedef std::tuple_element_t<I,TupleType> T;
    tlassert(I < args.size());
    return args[I].get_as<std::decay_t<T>>();
}

template <typename Func, typename Args, typename IndexList>
struct Applier;

template <typename Func, typename Tuple, size_t... I>
struct Applier<Func, Tuple, std::index_sequence<I...>> {
    template <typename... FreeArgs>
    static auto on(Func&& func, std::vector<Data>& args, FreeArgs&&... free_args) {
        return func(
            extract_data<I,Tuple>(args)...,
            std::forward<FreeArgs>(free_args)...
        );
    }
};

template <typename T>
struct ApplyArgsHelper {};

template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{
    template <typename F, typename... FreeArgs>
    static ReturnType run(F&& f, std::vector<Data>& args, FreeArgs&&... free_args) {
        return Applier<
            F,std::tuple<Args...>,
            std::make_index_sequence<sizeof...(Args) - sizeof...(FreeArgs)>
        >::on(std::forward<F>(f), args, std::forward<FreeArgs>(free_args)...);
    }
};

// Overload for const member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...) const>: 
    public ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{};

template <typename F>
struct ApplyArgsSpecializer {
    template <typename F2, typename... FreeArgs>
    static auto run(F2&& f, std::vector<Data>& args, FreeArgs&&... free_args) {
        return ApplyArgsHelper<
            decltype(&F::operator())
        >::run(std::forward<F2>(f), args, std::forward<FreeArgs>(free_args)...);
    }
};

template <typename Return, typename... Args>
struct ApplyArgsSpecializer<Function<Return(Args...)>> {
    template <typename F2, typename... FreeArgs>
    static auto run(F2&& f, std::vector<Data>& args, FreeArgs&&... free_args) {
        return ApplyArgsHelper<decltype(&Function<Return(Args...)>::call)>::run(
            std::forward<F2>(f), args, std::forward<FreeArgs>(free_args)...
        );
    }
};

template <typename F, typename... FreeArgs>
auto apply_data_args(F&& f, std::vector<Data>& args, FreeArgs&&... free_args) {
    return ApplyArgsSpecializer<std::decay_t<F>>::run(
        std::forward<F>(f), args, std::forward<FreeArgs>(free_args)...
    );
}

template <typename IndexList>
struct TupleApplier;

template <size_t... I>
struct TupleApplier<std::index_sequence<I...>> {
    template <typename Func, typename Tuple, typename... FreeArgs>
    static auto on(Func&& func, Tuple& args, FreeArgs&&... free_args) {
        return func(
            std::get<I>(args)...,
            std::forward<FreeArgs>(free_args)...
        );
    }
};

template <typename F, typename TupleT, typename... FreeArgs>
auto apply_args(F&& f, TupleT&& args, FreeArgs&&... free_args) {
    return TupleApplier<
        std::make_index_sequence<std::tuple_size<std::decay_t<TupleT>>::value>
    >::on(
        std::forward<F>(f),
        std::forward<TupleT>(args),
        std::forward<FreeArgs>(free_args)...
    );
}

} //end namespace taskloaf

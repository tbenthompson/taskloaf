#pragma once

#include <tuple>

#include "data.hpp"

namespace taskloaf {

template <size_t I, typename TupleType>
auto& extract_data(std::vector<Data>& args) {
    typedef typename std::tuple_element<I,TupleType>::type T;
    tlassert(I < args.size());
    return typename std::add_lvalue_reference<T>::type(
        args[I].get_as<typename std::decay<T>::type>()
    );
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
    static ReturnType run(std::vector<Data>& args, F&& f, FreeArgs&&... free_args) {
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
    static auto run(std::vector<Data>& args, F2&& f, FreeArgs&&... free_args) {
        return ApplyArgsHelper<
            decltype(&F::operator())
        >::run(args, std::forward<F2>(f), std::forward<FreeArgs>(free_args)...);
    }
};

template <typename Return, typename... Args>
struct ApplyArgsSpecializer<Function<Return(Args...)>> {
    template <typename F2, typename... FreeArgs>
    static auto run(std::vector<Data>& args, F2&& f, FreeArgs&&... free_args) {
        return ApplyArgsHelper<decltype(&Function<Return(Args...)>::call)>::run(
            args, std::forward<F2>(f), std::forward<FreeArgs>(free_args)...
        );
    }
};

template <typename F, typename... FreeArgs>
auto apply_args(std::vector<Data>& args, F&& f, FreeArgs&&... free_args) {
    return ApplyArgsSpecializer<
        typename std::decay<F>::type
    >::run(args,std::forward<F>(f), std::forward<FreeArgs>(free_args)...);
}

} //end namespace taskloaf

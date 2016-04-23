#pragma once

#include <tuple>
#include <cassert>

#include "data.hpp"

namespace taskloaf {

template <size_t index = 0, typename T>
auto build_input(std::vector<Data>& args) 
{
    assert(index < args.size());
    return std::tuple<typename std::add_lvalue_reference<T>::type>(
        args[index].get_as<typename std::decay<T>::type>()
    );
}

template <size_t index = 0, typename T, typename... Args,
          typename std::enable_if<sizeof...(Args) != 0,int>::type = 0>
auto build_input(std::vector<Data>& args) {
    return std::tuple_cat(
        build_input<index,T>(args),
        build_input<index+1,Args...>(args)
    );
}

//from: http://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
template <typename F, typename Tuple, bool Done, int Total, int... N>
struct apply_args_impl
{
    static auto call(F f, Tuple&& t) {
        return apply_args_impl<
            F, Tuple, Total == 1 + sizeof...(N),
            Total, N..., sizeof...(N)
        >::call(f, std::forward<Tuple>(t));
    }
};

template <typename F, typename Tuple, int Total, int... N>
struct apply_args_impl<F, Tuple, true, Total, N...>
{
    static auto call(F f, Tuple&& t) {
        return f(std::get<N>(std::forward<Tuple>(t))...);
    }
};

template <typename T>
struct ApplyArgsHelper {};

template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{
    template <typename F>
    static ReturnType run(std::vector<Data>& args, F f) {
        auto input = build_input<0,Args...>(args);
        typedef decltype(input) Tuple;
        typedef typename std::decay<decltype(input)>::type ttype;
        return apply_args_impl<
            F, decltype(input),
            0 == std::tuple_size<ttype>::value,
            std::tuple_size<ttype>::value
        >::call(f, std::forward<Tuple>(input));
    }
};

// Overload for const member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...) const>: 
    public ApplyArgsHelper<ReturnType(ClassType::*)(Args...)>
{};


template <typename CallableType, typename... Args>
auto apply_args(std::tuple<Args...>& args, const CallableType& f) {
    typedef typename std::decay<decltype(args)>::type ttype;
    return apply_args_impl<
        CallableType, decltype(args), 0 == std::tuple_size<ttype>::value,
        std::tuple_size<ttype>::value
    >::call(f, std::forward<decltype(args)>(args));
}

template <typename CallableType, typename ArgDeductionType>
struct ApplyArgsSpecializer {
    static auto run(std::vector<Data>& args, const CallableType& f) {
        return ApplyArgsHelper<decltype(&ArgDeductionType::operator())>::run(args, f);
    }
};

template <typename Return, typename... Args>
struct ApplyArgsSpecializer<Function<Return(Args...)>,Function<Return(Args...)>> {
    typedef Function<Return(Args...)> FType;
    static auto run(std::vector<Data>& args, const FType& f) {
        return ApplyArgsHelper<decltype(&FType::call)>::run(args, f);
    }
};

template <typename CallableType, typename ArgDeductionType = CallableType>
auto apply_args(std::vector<Data>& args, const CallableType& f) {
    return ApplyArgsSpecializer<CallableType,ArgDeductionType>::run(args,f);
}

} //end namespace taskloaf

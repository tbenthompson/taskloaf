#pragma once
#include <map>
#include <cassert>
#include <vector>
#include <typeindex>

#include "data.hpp"

namespace taskloaf {

template <size_t index = 0, typename T>
std::tuple<T> build_input(std::vector<Data>& args) 
{
    typedef typename std::remove_reference<T>::type TDeref;
    typedef typename std::remove_cv<TDeref>::type TDerefDeCV;
    assert(index < args.size());
    return std::tuple<T>(args[index].get_as<TDerefDeCV>());
}

template <size_t index = 0, typename T, typename... Args,
          typename std::enable_if<sizeof...(Args) != 0,int>::type = 0>
std::tuple<T, Args...> build_input(std::vector<Data>& args) 
{
    return std::tuple_cat(
        build_input<index,T>(args),
        build_input<index+1,Args...>(args)
    );
}

//from: http://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
template <typename F, typename Tuple, bool Done, int Total, int... N>
struct apply_args_impl
{
    static auto call(F f, Tuple && t)
    {
        return apply_args_impl<
            F, Tuple, Total == 1 + sizeof...(N),
            Total, N..., sizeof...(N)
        >::call(f, std::forward<Tuple>(t));
    }
};

template <typename F, typename Tuple, int Total, int... N>
struct apply_args_impl<F, Tuple, true, Total, N...>
{
    static auto call(F f, Tuple && t)
    {
        return f(std::get<N>(std::forward<Tuple>(t))...);
    }
};

template <typename T>
struct ApplyArgsHelper {};

template <typename ClassType, typename ReturnType, typename... Args>
struct ApplyArgsHelper<ReturnType(ClassType::*)(Args...) const>
{
    template <typename F>
    static ReturnType run(std::vector<Data>& args, F f) 
    {
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

template <typename CallableType, typename ArgDeductionType = CallableType>
auto apply_args(std::vector<Data>& args, const CallableType& f)
{
    return ApplyArgsHelper<decltype(&ArgDeductionType::operator())>::run(args, f);
}



//Modified from https://github.com/darabos/pinty/blob/master/pinty.h
struct Closure {
    template <typename Func>
    Closure(Func f) :
        data(reinterpret_cast<const char*>(new Func(std::move(f)))),
        size(sizeof(f))
    {}

    Closure(const char* data, size_t size):
        data(data),
        size(size) 
    {}

    const char* data;
    size_t size;
};

template <typename Return, typename Func, typename... Args>
struct Caller {
    static Return Call(const Closure& c, Args... args) {
        auto callable = *reinterpret_cast<const Func*>(c.data);
        return callable(
            std::forward<Args>(args)...
        );
    }
};

template <typename Signature> 
struct Function {
    typedef typename Function<decltype(&Signature::operator())>::ret ret;
};

template <typename Return, typename Class, typename... Args>
struct Function<Return (Class::*)(Args...) const>
{
    typedef Return ret;
};

template <typename Return, typename... Args>
struct Function<Return(Args...)> {
    typedef Return ret;
    typedef Return (*Call)(const Closure&, Args...);

    template <typename F>
    Function(F f):
        call(&Caller<Return,F,Args...>::Call),
        closure(f)
    {}

    Function(Call call, Closure closure):
        call(call),
        closure(closure) 
    {}

    Return operator()(Args... args) {
        return call(closure, std::forward<Args>(args)...);
    }

    Call call;
    Closure closure;
};

} //end namespace taskloaf

#pragma once
#include <map>
#include <cassert>
#include <vector>

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

template <typename CallableType, typename... Args>
auto apply_args(std::tuple<Args...>& args, const CallableType& f) {
    typedef typename std::decay<decltype(args)>::type ttype;
    return apply_args_impl<
        CallableType, decltype(args), 0 == std::tuple_size<ttype>::value,
        std::tuple_size<ttype>::value
    >::call(f, std::forward<decltype(args)>(args));
}

template <typename CallableType, typename ArgDeductionType = CallableType>
auto apply_args(std::vector<Data>& args, const CallableType& f)
{
    return ApplyArgsHelper<decltype(&ArgDeductionType::operator())>::run(args, f);
}


//Modified substantially from https://github.com/darabos/pinty/blob/master/pinty.h
template <typename T> 
struct Function {};

// This is a function wrapper similar to std::function with the additionally 
// ability to serialize and deserialize functions and closures.
// IMPORTANT!!! This only works for closures of POD data types.
// Use a future to pass a non-POD type to a task.
template <typename Return, typename... Args>
struct Function<Return(Args...)> {
    template <typename Func>
    struct Caller {
        static Return Call(const std::string& data, Args... args) {
            const char* char_arr = data.c_str();
            return (*const_cast<Func*>(reinterpret_cast<const Func*>(char_arr)))
                (std::forward<Args>(args)...);
        }
    };

    typedef Return (*Call)(const std::string&, Args...);
    const static size_t call_size = sizeof(Call);

    Function() = default;

    template <typename F>
    Function(F f):
        call(&Caller<F>::Call)
    {
        auto newf = std::make_unique<F>(std::move(f));
        closure = std::string(reinterpret_cast<const char*>(newf.get()), sizeof(f));
    }

    Return operator()(Args&&... args) {
        return call(closure, std::forward<Args>(args)...);
    }

    std::string to_string() const {
        auto call_ptr = reinterpret_cast<const char*>(&call);
        return std::string(call_ptr, call_size) + closure;
    }

    void from_string(const std::string& dump) {
        call = *const_cast<Call*>(reinterpret_cast<const Call*>(dump.c_str()));
        closure = dump.substr(sizeof(call));
    }

    template <typename Archive>
    void save(Archive& ar) const {
        ar(to_string());
    }

    template <typename Archive>
    void load(Archive& ar) {
        std::string serialized_fnc;
        ar(serialized_fnc);
        from_string(serialized_fnc);
    }

    Call call;
    std::string closure;
};

} //end namespace taskloaf

#pragma once
#include <map>
#include <cassert>
#include <vector>
#include <typeindex>

#include "data.hpp"

namespace taskloaf {

template <size_t index = 0, typename T>
std::tuple<T> build_input(const std::vector<Data*>& args) 
{
    typedef typename std::remove_reference<T>::type TDeref;
    typedef typename std::remove_cv<TDeref>::type TDerefDeCV;
    assert(index < args.size());
    return std::tuple<T>(args[index]->get_as<TDerefDeCV>());
}

template <size_t index = 0, typename T, typename... Args,
          typename std::enable_if<sizeof...(Args) != 0,int>::type = 0>
std::tuple<T, Args...> build_input(const std::vector<Data*>& args) 
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
    static ReturnType run(const std::vector<Data*>& args, F f) 
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
auto apply_args(const std::vector<Data*>& args, const CallableType& f)
{
    return ApplyArgsHelper<decltype(&ArgDeductionType::operator())>::run(args, f);
}

template <typename F>
struct CallFunctorByType
{
    template <typename... Args>
    auto operator()(Args&&... xs) const
    {
        return reinterpret_cast<const F&>(*this)(std::forward<Args>(xs)...);
    }
};

extern std::map<std::type_index,void(*)(const std::vector<Data*>&)> fnc_registry;

template <typename F>
struct RegisteredType
{
    static RegisteredType instance;
    std::type_index name;
    RegisteredType():
        name(std::type_index(typeid(F)))
    {
        auto caller = [] (const std::vector<Data*>& in) {
            CallFunctorByType<F> caller;
            caller(in);
        };
        fnc_registry[name] = caller;
    }
    static std::type_index get_name() {
        return instance.name; 
    }
};

template<typename F>
RegisteredType<F> RegisteredType<F>::instance;

template <typename F>
auto get_fnc_name(F f) {
    (void)f;
    return RegisteredType<F>::get_name(); 
}

template <typename Signature>
struct MakeFunctor;

template <typename Ret, typename... Args>
struct MakeFunctor<Ret(Args...)> {
    template <Ret (*F)(Args...)>
    struct from {
        Ret operator()(Args&&... args) const
        {
            return F(std::forward<Args>(args)...);
        }
    };
};

} //end namespace taskloaf

#pragma once
#include "tlassert.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>

#include <memory>

namespace taskloaf {
// Inspired by https://github.com/darabos/pinty/blob/master/pinty.h

template <typename F>
struct is_serializable: std::integral_constant<bool,
    cereal::traits::is_output_serializable<F,cereal::BinaryOutputArchive>::value &&
    cereal::traits::is_input_serializable<F,cereal::BinaryInputArchive>::value &&
    !std::is_pointer<F>::value> {};

struct RegistryImpl;
struct CallerRegistry {
    std::unique_ptr<RegistryImpl> impl;
    
    CallerRegistry();
    ~CallerRegistry();

    void insert(const std::type_info& t_info, void* f_ptr);
    std::pair<size_t,size_t> lookup_location(const std::type_info& t_info );
    void* get_function(const std::pair<size_t,size_t>& loc);
};
CallerRegistry& get_caller_registry();

template<typename Func, typename Return, typename... Args>
struct RegisterCaller
{
    RegisterCaller() {
        get_caller_registry().insert(typeid(Func), reinterpret_cast<void*>(call));
    }

    static Return call(const std::string& data, Args... args) {
        const char* char_arr = data.c_str();
        auto& closure = *const_cast<Func*>(
            reinterpret_cast<const Func*>(char_arr)
        );
        return closure(args...);
    }

    static RegisterCaller instance;
    static std::pair<size_t,size_t> add_to_registry() {
        (void)instance;
        return get_caller_registry().lookup_location(typeid(Func));
    }
};

template<typename Func, typename Return, typename... Args>
RegisterCaller<Func,Return,Args...> RegisterCaller<Func,Return,Args...>::instance;

template <typename T> 
struct Function {};

// This is a function wrapper similar to std::function with the 
// ability to serialize and deserialize functions and closures.
// IMPORTANT!!! This only works for closures of POD data types.
// Use a future to pass a non-POD type to a task.
template <typename Return, typename... Args>
struct Function<Return(Args...)> {
    typedef Return (*FncCaller)(const std::string&, Args...);

    std::pair<size_t,size_t> caller_id;
    std::string closure;

    Function() = default;

    template <
        typename F,
        typename std::enable_if<
            !std::is_same<Function<Return(Args...)>,F>::value
        >::type* = nullptr
    >
    Function(F f) {
        typedef typename std::decay<F>::type DecayF;
        static_assert(!is_serializable<DecayF>::value,
            "Do not use Function for serializable types");

        // This copy prevents a buggy "uninitialized" warning from happening in
        // gcc-5.2. ‘<anonymous>’ is used uninitialized in this function with
        // non-capturing lambdas. I suspect the copy will be optimized out so
        // there is no performance hit.
        auto newf = f;
        const char* data = reinterpret_cast<const char*>(&newf);
        closure = std::string(data, sizeof(F));
        caller_id = RegisterCaller<DecayF,Return,Args...>::add_to_registry();
    }

    Function(const Function<Return(Args...)>& f) = default;
    Function& operator=(const Function<Return(Args...)>& f) = default;
    Function(Function<Return(Args...)>&& f) = default;
    Function& operator=(Function<Return(Args...)>&& f) = default;

    Return operator()(Args... args) const {
        auto caller = reinterpret_cast<FncCaller>(
            get_caller_registry().get_function(caller_id)
        );
        // No forwarding so that everything becomes an lvalue reference
        return caller(closure, args...);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(caller_id);
        ar(closure);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(caller_id);
        ar(closure);
    }
};

// From http://stackoverflow.com/a/12283159 to auto convert many function-like
// things to std::function
template<typename T> struct RemoveClass { };
template<typename C, typename R, typename... A>
struct RemoveClass<R(C::*)(A...)> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct RemoveClass<R(C::*)(A...) const> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct RemoveClass<R(C::*)(A...) volatile> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct RemoveClass<R(C::*)(A...) const volatile> { using type = R(A...); };

template<typename T>
struct GetSignatureImpl {
    using type = typename RemoveClass<
        decltype(&std::remove_reference<T>::type::operator())
    >::type; 
};
template<typename R, typename... A>
struct GetSignatureImpl<R(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct GetSignatureImpl<R(&)(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct GetSignatureImpl<R(*)(A...)> { using type = R(A...); };

template <typename T>
using GetSignature = typename GetSignatureImpl<typename std::decay<T>::type>::type;

template <
    typename F,
    typename std::enable_if<
        !is_serializable<typename std::decay<F>::type>::value
    >::type* = nullptr>
Function<GetSignature<F>> make_function(F f) {
    return Function<GetSignature<F>>(f);
}

template <
    typename F,
    typename std::enable_if<
        is_serializable<typename std::decay<F>::type>::value
    >::type* = nullptr>
auto make_function(F&& f) {
    return std::forward<F>(f);
}

} //end namespace taskloaf

#pragma once
#include "tlassert.hpp"

#include <map>
#include <vector>
#include <memory>
#include <typeindex>

#include <cereal/types/utility.hpp>

namespace taskloaf {

// Inspired by https://github.com/darabos/pinty/blob/master/pinty.h
struct CallerRegistry {
    std::map<size_t,std::vector<std::pair<std::type_index,void*>>> registry;

    template <typename F>
    void insert(void* f_ptr) {
        auto tid = typeid(F).hash_code();
        registry[tid].push_back({std::type_index(typeid(F)), f_ptr});
    }

    template <typename F>
    std::pair<size_t,size_t> lookup_location() {
        auto tid = typeid(F).hash_code();
        for (size_t i = 0; i < registry[tid].size(); i++) {
            if (std::type_index(typeid(F)) == registry[tid][i].first) {
                return {tid, i};
            }
        }
        tlassert(false);
        return {0, 0};
    }

    void* get_function(const std::pair<size_t,size_t>& loc) {
        return registry[loc.first][loc.second].second;
    }
};

inline auto& get_caller_registry() {
    static CallerRegistry caller_registry;
    return caller_registry;
}

template<typename Return, typename... Args>
using FncCaller = Return (*)(const std::string&, Args...);

template<typename Func, typename Return, typename... Args>
struct RegisterCaller
{
    RegisterCaller() {
        auto f = [] (const std::string& data, Args... args) {
            const char* char_arr = data.c_str();
            auto& closure = *const_cast<Func*>(
                reinterpret_cast<const Func*>(char_arr)
            );
            return closure(args...);
        };
        auto f_ptr = static_cast<FncCaller<Return,Args...>>(f);
        get_caller_registry().insert<Func>(reinterpret_cast<void*>(f_ptr));
    }
    static RegisterCaller instance;
    static std::pair<size_t,size_t> add_to_registry() {
        (void)instance;
        return get_caller_registry().lookup_location<Func>();
    }
};

template<typename Func, typename Return, typename... Args>
RegisterCaller<Func,Return,Args...> RegisterCaller<Func,Return,Args...>::instance;

template <typename Func, typename Return, typename... Args>
static auto get_call() {
    return RegisterCaller<Func,Return,Args...>::add_to_registry();
}

template <typename T> 
struct Function {};

// This is a function wrapper similar to std::function with the 
// ability to serialize and deserialize functions and closures.
// IMPORTANT!!! This only works for closures of POD data types.
// Use a future to pass a non-POD type to a task.
template <typename Return, typename... Args>
struct Function<Return(Args...)> {
    std::pair<size_t,size_t> caller_id;
    std::string closure;

    Function() = default;

    template <typename F>
    Function(F f) {
        caller_id = get_call<F,Return,Args...>();
        auto newf = std::unique_ptr<F>(new F(std::move(f)));
        closure = std::string(reinterpret_cast<const char*>(newf.get()), sizeof(f));
    }

    template <typename... As>
    Return operator()(As&&... args) const {
        auto caller = reinterpret_cast<FncCaller<Return,Args...>>(
            get_caller_registry().get_function(caller_id)
        );
        return caller(closure, std::forward<As>(args)...);
    }

    Return call(Args... args) {
        return this->operator()(args...);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(caller_id);
        ar(closure);
    }
};

// From http://stackoverflow.com/a/12283159 to auto convert many function-like
// things to std::function
template<typename T> struct remove_class { };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...)> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) volatile> { using type = R(A...); };
template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const volatile> { using type = R(A...); };

template<typename T>
struct get_signature_impl { using type = typename remove_class<
    decltype(&std::remove_reference<T>::type::operator())>::type; };
template<typename R, typename... A>
struct get_signature_impl<R(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct get_signature_impl<R(&)(A...)> { using type = R(A...); };
template<typename R, typename... A>
struct get_signature_impl<R(*)(A...)> { using type = R(A...); };
template<typename T> using get_signature =
    typename get_signature_impl<typename std::decay<T>::type>::type;

template <typename F>
auto make_function(F&& f) {
    return Function<get_signature<F>>(std::forward<F>(f));
}

} //end namespace taskloaf

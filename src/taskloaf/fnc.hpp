#pragma once
#include <map>
#include <cassert>
#include <vector>

#include "data.hpp"

namespace taskloaf {

//Modified substantially from https://github.com/darabos/pinty/blob/master/pinty.h
template <typename T> 
struct Function {};

template <typename Return, typename... Args>
struct Function<Return(Args...)> {
    typedef Return (*Call)(Args...);
    const static size_t call_size = sizeof(Call);

    Function() = default;

    template <typename F>
    Function(F f):
        fnc(reinterpret_cast<intptr_t>(static_cast<Call>(f)))
    {}

    Return operator()(Args... args) const {
        return reinterpret_cast<Call>(fnc)(std::forward<Args>(args)...);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(fnc);
    }

    intptr_t fnc;
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

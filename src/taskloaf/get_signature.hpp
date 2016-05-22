#pragma once
#include <type_traits>

namespace taskloaf {

// From http://stackoverflow.com/a/12283159 to determine the signature of
// most function-like types
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

}

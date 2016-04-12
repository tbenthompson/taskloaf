#pragma once
#include <map>
#include <cassert>
#include <vector>
#include <memory>

namespace taskloaf {

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

    Function(const std::string& dump) {
        from_string(dump);
    }

    Return operator()(Args... args) {
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

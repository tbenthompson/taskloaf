#pragma once

#include "fnc.hpp"

namespace taskloaf {

template <typename T> 
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args...)> fnc;
    std::vector<Data> input;

    template <typename F>
    Closure(F f, std::vector<Data> args):
        fnc(f),
        input(std::move(args))
    {}

    Return operator()(Args... a) {
        return fnc(input, a...);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(fnc);
        ar(input);
    }
};

typedef Closure<Data(std::vector<Data>&)> PureTaskT;
typedef Closure<void(std::vector<Data>& val)> TriggerT;
typedef Closure<void()> TaskT;

} //end namespace taskloaf

// Ensure that reference_wrappers can be serialized. This is for testing 
// purposes only and should only be used when the reference will not outlast
// what it refers to. 
namespace cereal {
template <typename T, typename Archive>
void serialize(Archive& ar, std::reference_wrapper<T>& r) {
    (void)ar; (void)r;
}
}


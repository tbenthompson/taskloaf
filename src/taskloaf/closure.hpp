#pragma once

#include "fnc.hpp"
#include "data.hpp"

#include <cereal/types/vector.hpp>

namespace taskloaf {

template <typename T> 
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args...)> fnc;
    std::vector<Data> input;

    Closure() = default;

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


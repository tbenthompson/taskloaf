#pragma once

#include "fnc.hpp"
#include "data.hpp"
#include "execute.hpp"

#include <cereal/types/vector.hpp>

namespace taskloaf {

template <typename T> 
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args&...)> fnc;
    std::vector<Data> input;

    Closure() = default;

    Return operator()(Args... args) {
        return fnc(input, args...);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(fnc);
        ar(input);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(fnc);
        ar(input);
    }
};

typedef Closure<Data(std::vector<Data>&)> PureTaskT;
typedef Closure<void(std::vector<Data>&)> TriggerT;
typedef Closure<void()> TaskT;

} //end namespace taskloaf

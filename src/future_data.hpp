#pragma once

#include <memory>

#include "fnc.hpp"
#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

struct FutureData {
    //Force polymorphism through a single virtual function.
    virtual void nullfnc() {};
};

struct Then: public FutureData {
    template <typename F>
    Then(std::shared_ptr<FutureData> fut, F fnc):
        child(fut),
        fnc_name(get_fnc_name(fnc))
    {}

    std::shared_ptr<FutureData> child;
    std::string fnc_name;
};

struct Unwrap: public FutureData {
    Unwrap(std::shared_ptr<FutureData> fut):
        child(fut)
    {}

    std::shared_ptr<FutureData> child;
};

struct Async: public FutureData {
    template <typename F>
    Async(F fnc):
        fnc_name(get_fnc_name(fnc))
    {}

    std::string fnc_name;
};

struct Ready: public FutureData {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    {}

    SafeVoidPtr data;
};

struct WhenAll: public FutureData {
    WhenAll(std::vector<std::shared_ptr<FutureData>> args):
        children(args)
    {}

    std::vector<std::shared_ptr<FutureData>> children;
};

} //end namespace taskloaf

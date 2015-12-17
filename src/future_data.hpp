#pragma once

#include <memory>

#include "fnc.hpp"
#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

struct FutureData {};

struct Then: public FutureData {
    template <typename F>
    Then(std::shared_ptr<FutureData> fut, F fnc):
        fut(fut),
        fnc_name(get_fnc_name(fnc))
    {}

    std::shared_ptr<FutureData> fut;
    std::string fnc_name;
};

struct Unwrap: public FutureData {
    Unwrap(std::shared_ptr<FutureData> fut):
        fut(fut)
    {}

    std::shared_ptr<FutureData> fut;
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
        data(args)
    {}

    std::vector<std::shared_ptr<FutureData>> data;
};

} //end namespace taskloaf

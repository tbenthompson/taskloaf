#pragma once

#include <memory>

#include "fnc.hpp"
#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

enum FutureDataType {
    ThenType, UnwrapType, AsyncType, ReadyType, WhenAllType
};

struct FutureData {
    FutureDataType type;

    virtual ~FutureData() {}
};

struct Then: public FutureData {
    template <typename F>
    Then(std::shared_ptr<FutureData> fut, F fnc):
        child(fut),
        fnc_name(get_fnc_name(fnc))
    { type = ThenType; }

    std::shared_ptr<FutureData> child;
    std::type_index fnc_name;
};

struct Unwrap: public FutureData {
    template <typename F>
    Unwrap(std::shared_ptr<FutureData> fut, F fnc):
        child(fut),
        fnc_name(get_fnc_name(fnc))
    { type = UnwrapType; }

    std::shared_ptr<FutureData> child;
    std::type_index fnc_name;
};

struct Async: public FutureData {
    template <typename F>
    Async(F fnc):
        fnc_name(get_fnc_name(fnc))
    { type = AsyncType; }

    std::type_index fnc_name;
};

struct Ready: public FutureData {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    { type = ReadyType; }

    SafeVoidPtr data;
};

struct WhenAll: public FutureData {
    WhenAll(std::vector<std::shared_ptr<FutureData>> args):
        children(args)
    { type = WhenAllType; }

    std::vector<std::shared_ptr<FutureData>> children;
};

} //end namespace taskloaf

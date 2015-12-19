#pragma once

#include <memory>

#include "fnc.hpp"
#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

enum FutureNodeType {
    ThenType, UnwrapType, AsyncType, ReadyType, WhenAllType
};

struct FutureNode {
    FutureNodeType type;

    virtual ~FutureNode() {}
};

struct Then: public FutureNode {
    template <typename F>
    Then(std::shared_ptr<FutureNode> fut, F fnc):
        child(fut),
        fnc_name(get_fnc_name(fnc))
    { type = ThenType; }

    std::shared_ptr<FutureNode> child;
    std::type_index fnc_name;
};

struct Unwrap: public FutureNode {
    template <typename F>
    Unwrap(std::shared_ptr<FutureNode> fut, F fnc):
        child(fut),
        fnc_name(get_fnc_name(fnc))
    { type = UnwrapType; }

    std::shared_ptr<FutureNode> child;
    std::type_index fnc_name;
};

struct Async: public FutureNode {
    template <typename F>
    Async(F fnc):
        fnc_name(get_fnc_name(fnc))
    { type = AsyncType; }

    std::type_index fnc_name;
};

struct Ready: public FutureNode {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    { type = ReadyType; }

    SafeVoidPtr data;
};

struct WhenAll: public FutureNode {
    WhenAll(std::vector<std::shared_ptr<FutureNode>> args):
        children(args)
    { type = WhenAllType; }

    std::vector<std::shared_ptr<FutureNode>> children;
};

} //end namespace taskloaf

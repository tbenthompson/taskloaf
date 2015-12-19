#pragma once

#include <memory>
#include <vector>

#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

typedef std::function<Data(std::vector<Data>&)> InputTaskT;

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
        fnc(fnc)
    { type = ThenType; }

    const std::shared_ptr<FutureNode> child;
    const InputTaskT fnc;
};

struct Unwrap: public FutureNode {
    template <typename F>
    Unwrap(std::shared_ptr<FutureNode> fut, F fnc):
        child(fut),
        fnc(fnc)
    { type = UnwrapType; }

    const std::shared_ptr<FutureNode> child;
    const InputTaskT fnc;
};

struct Async: public FutureNode {
    template <typename F>
    Async(F fnc):
        fnc(fnc)
    { type = AsyncType; }

    const InputTaskT fnc;
};

struct Ready: public FutureNode {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    { type = ReadyType; }

    const SafeVoidPtr data;
};

struct WhenAll: public FutureNode {
    WhenAll(std::vector<std::shared_ptr<FutureNode>> args):
        children(args)
    { type = WhenAllType; }

    const std::vector<std::shared_ptr<FutureNode>> children;
};

} //end namespace taskloaf

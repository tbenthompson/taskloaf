#pragma once

#include <memory>
#include <vector>

#include "data.hpp"

namespace taskloaf {

typedef std::function<Data(std::vector<Data>&)> InputTaskT;

enum FutureNodeType {
    ThenType, UnwrapType, AsyncType, ReadyType, WhenAllType
};

struct FutureNode {
    FutureNodeType type;

    virtual ~FutureNode() {}
};

struct Then: public FutureNode {
    Then(std::shared_ptr<FutureNode> fut, InputTaskT fnc):
        child(fut),
        fnc(fnc)
    { type = ThenType; }

    const std::shared_ptr<FutureNode> child;
    const InputTaskT fnc;
};

struct Unwrap: public FutureNode {
    Unwrap(std::shared_ptr<FutureNode> fut, InputTaskT fnc):
        child(fut),
        fnc(fnc)
    { type = UnwrapType; }

    const std::shared_ptr<FutureNode> child;
    const InputTaskT fnc;
};

struct Async: public FutureNode {
    Async(InputTaskT fnc):
        fnc(fnc)
    { type = AsyncType; }

    const InputTaskT fnc;
};

struct Ready: public FutureNode {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(std::move(val)))
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

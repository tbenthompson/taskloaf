#pragma once

#include <memory>

#include "fnc.hpp"
#include "data.hpp"
#include "future_visitor.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

struct FutureData {
    virtual void accept(FutureVisitor* visitor) = 0;
};

template <typename Derived>
struct VisitableFutureData: public FutureData {
    void accept(FutureVisitor* visitor) {
        visitor->visit(static_cast<Derived*>(this));
    }
};

struct Then: public VisitableFutureData<Then> {
    template <typename F>
    Then(std::shared_ptr<FutureData> fut, F fnc):
        fut(fut),
        fnc_name(get_fnc_name(fnc))
    {}

    std::shared_ptr<FutureData> fut;
    std::string fnc_name;
};

struct Unwrap: public VisitableFutureData<Unwrap> {
    Unwrap(std::shared_ptr<FutureData> fut):
        fut(fut)
    {}

    std::shared_ptr<FutureData> fut;
};

struct Async: public VisitableFutureData<Async> {
    template <typename F>
    Async(F fnc):
        fnc_name(get_fnc_name(fnc))
    {}

    std::string fnc_name;
};

struct Ready: public VisitableFutureData<Ready> {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    {}

    SafeVoidPtr data;
};

struct WhenAll: public VisitableFutureData<WhenAll> {
    WhenAll(std::vector<std::shared_ptr<FutureData>> args):
        data(args)
    {}

    std::vector<std::shared_ptr<FutureData>> data;
};

} //end namespace taskloaf

#pragma once

#include <cassert>

#include "fnc.hpp"
#include "future_node.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F fnc) {
    typedef std::result_of_t<F(Ts&...)> Return;
    std::function<Return(Ts&...)> container_fnc(std::move(fnc));
    auto task = [container_fnc] (std::vector<Data>& in) {
        return Data{make_safe_void_ptr(apply_args(in, container_fnc))};
    };
    return Future<Return>{std::make_shared<Then>(fut.data, task)};
}

template <typename T>
auto unwrap(const Future<T>& fut) {
    auto unwrapper = [] (std::vector<Data>& in) {
        return Data{make_safe_void_ptr(in[0].get_as<T>().data)};
    };
    return Future<typename T::type>{
        std::make_shared<Unwrap>(fut.data, unwrapper)
    };
}

template <typename T>
auto ready(T val) {
    return Future<T>{std::make_shared<Ready>(std::move(val))};
}

template <typename F>
auto async(F fnc) {
    auto task = [fnc] (std::vector<Data>& in) {
        (void)in;
        return Data{make_safe_void_ptr(fnc())};
    };
    return Future<std::result_of_t<F()>>{
        std::make_shared<Async>(task)
    };
}

template <typename... Ts>
auto when_all(const Future<Ts>&... args) {
    std::vector<std::shared_ptr<FutureNode>> data{args.data...};
    return Future<Ts...>{
        std::make_shared<WhenAll>(std::move(data))
    };
}


template <typename... Ts>
struct Future {
    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F f) const {
        return taskloaf::then(*this, f);
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F f) const {
        return taskloaf::then(*this, f);
    }
    
    auto unwrap() const {
        return taskloaf::unwrap(*this);
    }
};

} //end namespace taskloaf

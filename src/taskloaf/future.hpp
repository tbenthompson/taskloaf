#pragma once

#include <cassert>

#include "fnc.hpp"
#include "future_node.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future {
    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F f) const {
        typedef std::result_of_t<F(Ts...)> Return;
        std::function<Return(Ts...)> fnc(std::move(f));
        auto task = [fnc] (std::vector<Data>& in) {
            return Data{make_safe_void_ptr(apply_args(in, fnc))};
        };
        return Future<Return>{std::make_shared<Then>(data, task)};
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F f) const {
        typedef std::result_of_t<F(T&)> Return;
        std::function<std::result_of_t<F(T&)>(T&)> fnc(std::move(f));
        auto task = [fnc] (std::vector<Data>& in) {
            return Data{make_safe_void_ptr(apply_args(in, fnc))};
        };
        return Future<Return>{std::make_shared<Then>(data, task)};
    }
    
    auto unwrap() const {
        auto unwrapper = [] (std::vector<Data>& in) {
            return Data{make_safe_void_ptr(in[0].get_as<T>().data)};
        };
        return Future<typename T::type>{
            std::make_shared<Unwrap>(data, unwrapper)
        };
    }
};

template <typename T>
auto ready(T val) {
    return Future<T>{std::make_shared<Ready>(val)};
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

} //end namespace taskloaf

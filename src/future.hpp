#pragma once

#include <cassert>
#include <iostream>

#include "future_node.hpp"

namespace taskloaf {

#define TASKLOAF_FUNCTOR(f) MakeFunctor<decltype(f)>::from<f>{}

template <typename... Ts>
struct Future {
    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F fnc) {
        (void)fnc;
        auto task = [] (std::vector<Data>& in) {
            CallFunctorByType<F> f;
            return Data{make_safe_void_ptr(apply_args<decltype(f),F>(in, f))};
        };
        return Future<std::result_of_t<F(Ts...)>>{
           std::make_shared<Then>(data, task)
        };
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    std::shared_ptr<FutureNode> data;

    template <typename F>
    auto then(F fnc) {
        (void)fnc;
        auto task = [] (std::vector<Data>& in) {
            CallFunctorByType<F> f;
            return Data{make_safe_void_ptr(apply_args<decltype(f),F>(in, f))};
        };
        return Future<std::result_of_t<F(T)>>{
            std::make_shared<Then>(data, task)
        };
    }
    
    auto unwrap() {
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
    (void)fnc;
    auto task = [] (std::vector<Data>& in) {
        (void)in;
        CallFunctorByType<F> f;
        return Data{make_safe_void_ptr(f())};
    };
    return Future<std::result_of_t<F()>>{
        std::make_shared<Async>(task)
    };
}

// template <typename T>
// auto when_all(const Future<T>& arg1) {
//     return arg1;
// }

template <typename T, typename... Ts>
auto when_all(const Future<T>& arg1, const Future<Ts>&... args) {
    // arg1.then([=] (T val) { 
    //     return when_all(args);
    // }).unwrap();
    std::vector<std::shared_ptr<FutureNode>> data{arg1.data, args.data...};
    return Future<T, Ts...>{
        std::make_shared<WhenAll>(std::move(data))
    };
}

} //end namespace taskloaf

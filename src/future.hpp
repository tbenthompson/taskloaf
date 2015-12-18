#pragma once

#include <cassert>
#include <iostream>

#include "future_data.hpp"

namespace taskloaf {

#define TASKLOAF_FUNCTOR(f) MakeFunctor<decltype(f)>::from<f>{}

template <typename... Ts>
struct Future {
    std::shared_ptr<FutureData> data;

    template <typename F>
    auto then(F fnc) {
        (void)fnc;
        auto task = [] (const std::vector<Data*>& in) {
            CallFunctorByType<F> f;
            in.back()->unserialized_data = make_safe_void_ptr(
                apply_args<decltype(f),F>(in, f)
            );
        };
        return Future<std::result_of_t<F(Ts...)>>{
           std::make_shared<Then>(data, task)
        };
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    std::shared_ptr<FutureData> data;

    template <typename F>
    auto then(F fnc) {
        (void)fnc;
        auto task = [] (const std::vector<Data*>& in) {
            CallFunctorByType<F> f;
            in.back()->unserialized_data = make_safe_void_ptr(
                apply_args<decltype(f),F>(in, f)
            );
        };
        return Future<std::result_of_t<F(T)>>{
            std::make_shared<Then>(data, task)
        };
    }
    
    auto unwrap() {
        auto unwrapper = [] (const std::vector<Data*>& in) {
            in.back()->unserialized_data = make_safe_void_ptr(
                in[0]->get_as<T>().data
            );
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
    auto task = [] (const std::vector<Data*>& in) {
        CallFunctorByType<F> f;
        in.back()->unserialized_data = make_safe_void_ptr(f());
    };
    return Future<std::result_of_t<F()>>{
        std::make_shared<Async>(task)
    };
}

template <typename... Ts>
auto when_all(const Future<Ts>&... args) {
    std::vector<std::shared_ptr<FutureData>> data{args.data...};
    return Future<Ts...>{
        std::make_shared<WhenAll>(std::move(data))
    };
}

} //end namespace taskloaf

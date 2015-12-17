#pragma once

#include <cassert>

#include "future_data.hpp"

namespace taskloaf {

#define TASKLOAF_FUNCTOR(f) MakeFunctor<decltype(f)>::from<f>{}

template <typename... Ts>
struct Future {
    std::shared_ptr<FutureData> data;

    template <typename F>
    auto then(F fnc) {
        (void)fnc;
        auto task = [] (std::vector<Data*>& in) {
            CallFunctorByType<F> f;
            return apply_args<decltype(f),F>(in, f); 
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
        auto task = [] (std::vector<Data*>& in) {
            CallFunctorByType<F> f;
            return apply_args<decltype(f),F>(in, f); 
        };
        return Future<std::result_of_t<F(T)>>{
            std::make_shared<Then>(data, task)
        };
    }
    
    auto unwrap() {
        return Future<typename T::type>{
            std::make_shared<Unwrap>(data)
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
    auto task = [] (std::vector<Data*>& in) {
        (void)in;
        CallFunctorByType<F> f;
        return f();
    };
    return Future<std::result_of_t<F()>>{
        std::make_shared<Async>(task)
    };
}

template <typename... Ts>
auto when_all(Future<Ts>... args) {
    std::vector<std::shared_ptr<FutureData>> data{args.data...};
    return Future<Ts...>{
        std::make_shared<WhenAll>(std::move(data))
    };
}

} //end namespace taskloaf

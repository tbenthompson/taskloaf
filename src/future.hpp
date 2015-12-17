#pragma once

#include <memory>
#include <cassert>

#include "fnc.hpp"
#include "data.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future;

struct FutureData {};

struct Then: public FutureData {
    template <typename F>
    Then(std::shared_ptr<FutureData> fut, F fnc):
        fut(fut),
        fnc_name(get_fnc_name(fnc))
    {}

    std::shared_ptr<FutureData> fut;
    std::string fnc_name;
};

struct Unwrap: public FutureData {
    Unwrap(std::shared_ptr<FutureData> fut):
        fut(fut)
    {}

    std::shared_ptr<FutureData> fut;
};

struct Async: public FutureData {
    template <typename F>
    Async(F fnc):
        fnc_name(get_fnc_name(fnc))
    {}

    std::string fnc_name;
};

struct Ready: public FutureData {
    template <typename T>
    Ready(T val):
        data(make_safe_void_ptr(val))
    {}

    SafeVoidPtr data;
};

struct WhenAll: public FutureData {
    WhenAll(std::vector<std::shared_ptr<FutureData>> args):
        data(args)
    {}

    std::vector<std::shared_ptr<FutureData>> data;
};

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
        std::make_shared<WhenAll>(data)
    };
}

} //end namespace taskloaf

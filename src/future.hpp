#pragma once

#include <memory>
#include <type_traits>

struct SameProcess {
    
};

template <typename... Ts>
struct Future;

template <typename... Ts>
struct FutureData {};

template <typename F, typename... Ts>
struct Then: public FutureData<std::result_of_t<F(Ts...)>> {
    Then(std::shared_ptr<FutureData<Ts...>> fut, F fnc):
        fut(fut),
        fnc(std::move(fnc))
    {}

    std::shared_ptr<FutureData<Ts...>> fut;
    F fnc;
};

template <typename T>
struct Unwrap: public FutureData<T> {
    Unwrap(std::shared_ptr<FutureData<Future<T>>> fut):
        fut(fut)
    {}

    std::shared_ptr<FutureData<Future<T>>> fut;
};

template <typename F>
struct Async: public FutureData<std::result_of_t<F()>> {
    Async(F fnc):
        fnc(std::move(fnc))
    {}

    F fnc;
};

template <typename T>
struct Ready: public FutureData<T> {
    Ready(T val):
        val(val)
    {}

    T val;
};

template <typename... Ts>
struct WhenAll: public FutureData<Ts...> {
    WhenAll(std::shared_ptr<FutureData<Ts>>... args):
        data(args...)
    {}

    std::tuple<std::shared_ptr<FutureData<Ts>>...> data;
};

template <typename... Ts>
struct Future {
    std::shared_ptr<FutureData<Ts...>> data;

    template <typename F>
    auto then(F fnc) {
        return Future<std::result_of_t<F(Ts...)>>{
           std::make_shared<Then<F,Ts...>>(data, fnc)
        };
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    std::shared_ptr<FutureData<T>> data;

    template <typename F>
    auto then(F fnc) {
        return Future<std::result_of_t<F(T)>>{
            std::make_shared<Then<F,T>>(data, fnc)
        };
    }
    
    auto unwrap() {
        return Future<typename T::type>{
            std::make_shared<Unwrap<typename T::type>>(data)
        };
    }
};

template <typename T>
auto ready(T val) {
    return Future<T>{std::make_unique<Ready<T>>(val)};
}

template <typename F>
auto async(F fnc) {
    return Future<std::result_of_t<F()>>{
        std::make_unique<Async<F>>(std::move(fnc))
    };
}

template <typename... Ts>
auto when_all(Future<Ts>... args) {
    return Future<Ts...>{
        std::make_shared<WhenAll<Ts...>>(args.data...)
    };
}

// template <typename T>
// struct Run {};
// 
// template <typename T>
// struct Run<Ready<T>> {
//     T go(Ready<T> r) {
//         return 
//     }
// };

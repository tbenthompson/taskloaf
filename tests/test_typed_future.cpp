#include "catch.hpp"

#include "taskloaf.hpp"

using namespace taskloaf;

template <typename F, typename TupleT, size_t... I>
auto apply_args(F&& func, data& val, TupleT& args, std::index_sequence<I...>) {
    return func(std::move(std::get<I>(args))..., val);
}

template <typename F, typename TupleT, size_t... I>
auto apply_args(F&& func, TupleT& args, std::index_sequence<I...>) {
    return func(std::move(std::get<I>(args))...);
}

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

template <typename T>
struct tfuture {
    std::unique_ptr<future> fut;
    T val;

    tfuture(): fut(nullptr), val() {}
    tfuture(T val): fut(nullptr), val(std::move(val)) {}
    tfuture(future fut): fut(new future(std::move(fut))), val() {}

    tfuture(const tfuture& other) {
        val = other.val;
        if (unlikely(other.fut != nullptr)) {
            fut = std::make_unique<future>(*other.fut);
        }
    }

    tfuture& operator=(const tfuture& other) {
        val = other.val;
        if (unlikely(other.fut != nullptr)) {
            fut = std::make_unique<future>(*other.fut);
        }
        return *this;
    }


    template <typename F, typename... TEnclosed>
    auto then(F f, TEnclosed&&... enclosed_vals) {

        using result_type = std::result_of_t<F(TEnclosed...,T)>; 
        bool run_now = cur_worker->should_run_now();

        if (likely(!fut && run_now)) {
            return tfuture<result_type>(
                f(std::forward<TEnclosed>(enclosed_vals)..., val)
            );
        } else if (!fut) {
            return tfuture<result_type>(async(closure(
                [] (std::tuple<data,std::tuple<TEnclosed...,T>>& p,_) {
                    return apply_args(
                        std::get<0>(p).template get<F>(),
                        std::get<1>(p), std::index_sequence_for<TEnclosed...,T>{}
                    );
                },
                std::make_tuple(
                    data(std::move(f)),
                    std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...,val)
                )
            )));
        }
        // if (likely(run_now && fut == nullptr)) {
        //     return tfuture<result_type>(
        //         f(std::forward<TEnclosed>(enclosed_vals)..., val)
        //     );

        // } else if (run_now && fut->fulfilled_here()) {
        //     return tfuture<result_type>(
        //         f(std::forward<TEnclosed>(enclosed_vals)..., fut->get())
        //     );

        // } else if (fut == nullptr) {
        //     return tfuture<result_type>(async(closure(
        //         [] (std::tuple<data,std::tuple<TEnclosed...,T>>& p,_) {
        //             return apply_args(
        //                 std::get<0>(p).template get<F>(),
        //                 std::get<1>(p), std::index_sequence_for<TEnclosed...,T>{}
        //             );
        //         },
        //         std::make_tuple(
        //             data(std::move(f)),
        //             std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...,val)
        //         )
        //     )));
        // }
        // } else if (fut->fulfilled_here()) {
        //     return tfuture<result_type>(async(closure(
        //         [] (std::tuple<data,std::tuple<TEnclosed...,T>>& p,_) {
        //             return apply_args(
        //                 std::get<0>(p).template get<F>(),
        //                 std::get<1>(p), std::index_sequence_for<TEnclosed...,T>{}
        //             );
        //         },
        //         std::make_tuple(
        //             data(std::move(f)),
        //             std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...,fut->get())
        //         )
        //     )));

        return tfuture<result_type>(fut->then(closure(
            [] (std::tuple<data,std::tuple<TEnclosed...>>& p, data& d) {
                return apply_args(
                    std::get<0>(p).template get<F>(),
                    d, std::get<1>(p), std::index_sequence_for<TEnclosed...>{}
                );
            },
            std::make_tuple(
                data(std::move(f)),
                std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...)
            )
        )));
    }

    T unwrap_delayed() {
        return T(fut->then([] (_,T& inner_fut) { 
            if (inner_fut.fut == nullptr) {
                return ready(inner_fut.val);     
            } else {
                return *inner_fut.fut; 
            }
        }).unwrap());
    }

    T unwrap() & {
        // static_assert(std
        if (likely(!fut)) {
            return val;
        // } else if (fut->fulfilled_here()) {
        //     return fut->get(); 
        } else {
            return unwrap_delayed();
        }
    }

    T unwrap() && {
        if (likely(!fut)) {
            return std::move(val);
        // } else if (fut->fulfilled_here()) {
        //     return fut->get(); 
        } else {
            return unwrap_delayed();
        }
    }

    T& get() & {
        if (!fut) {
            return val;
        } else {
            return fut->get();
        }
    }

    T get() && {
        if (!fut) {
            return std::move(val);
        } else {
            return fut->get();
        }
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

template <typename T>
auto tready(T&& val) {
    return tfuture<std::decay_t<T>>(std::forward<T>(val));
}

template <typename F, typename... TEnclosed>
auto tasync(F f, TEnclosed&&... enclosed_vals) {
    using result_type = std::result_of_t<F(TEnclosed&...)>;

    bool run_now = cur_worker->should_run_now();
    if (likely(run_now)) {
        return tfuture<result_type>(f());
    }
    return tfuture<result_type>(async(closure(
        [] (std::tuple<data,std::tuple<TEnclosed...>>& p,_) {
            return apply_args(
                std::get<0>(p).template get<F>(),
                std::get<1>(p), std::index_sequence_for<TEnclosed...>{}
            );
        },
        std::make_tuple(
            data(std::move(f)),
            std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...)
        )
    )));
}

TEST_CASE("Ready immediate") {
    auto ctx = launch_local(1);
    REQUIRE(tready(10).get() == 10);
}

TEST_CASE("Async immediate") {
    auto ctx = launch_local(1);
    REQUIRE(tasync([] { return 10; }).get() == 10);
}

TEST_CASE("Then immediate") {
    auto ctx = launch_local(1);
    REQUIRE(tready(10).then([] (int x) { return x * 2; }).get() == 20);
}

TEST_CASE("Then immediate with enclosed") {
    auto ctx = launch_local(1);
    int result = tready(10).then(
        [] (int a, int x) { return a + x * 2; },
        2
    ).get();
    REQUIRE(result == 22);
}

tfuture<int> fib(int idx) {
    if (idx < 3) {
        return tready(1);
    } else {
        // return tasync([idx] () {
        //     return fib(idx - 1).then([] (tfuture<int> a, int x) {
        //         return a.then([x] (int y) { return x + y; });
        //     }, fib(idx - 2)).unwrap();
        // }).unwrap();
        // return tasync([=] { return fib(idx - 1); }).unwrap().then(
        //     [] (tfuture<int> a, int x) {
        //         return a.then([x] (int y) { return x + y; });
        //     },
        //     fib(idx - 2)
        // ).unwrap();
        return fib(idx - 1).then(
            [] (tfuture<int> a, int x) {
                return a.then([x] (int y) { return x + y; });
            },
            fib(idx - 2)
        ).unwrap();
    }
}

TEST_CASE("Fib") {
    config cfg;
    cfg.print_stats = true;
    auto ctx = launch_local(1,cfg);
    std::cout << int(fib(44).get()) << std::endl; 
}

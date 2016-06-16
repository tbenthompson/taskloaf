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

template <typename T>
struct tfuture {
    union {
        T val;
        future fut;
    };
    bool immediate;

    tfuture(): val(), immediate(true) {}
    tfuture(T val): val(std::move(val)), immediate(true) {}
    tfuture(future fut): fut(fut), immediate(false) {}

    tfuture(const tfuture& other) {
        copy_from(other);
    }

    tfuture& operator=(const tfuture& other) {
        destroy();
        copy_from(other);
        return *this;
    }

    tfuture(tfuture&& other) {
        move_from(other);
    }

    tfuture& operator=(tfuture&& other) {
        destroy();
        move_from(other);
        return *this;
    }

    ~tfuture() {
        destroy();
    }

    void copy_from(const tfuture& other) {
        immediate = other.immediate;
        if (immediate) {
            val = other.val; 
        } else {
            fut = other.fut;
        }
    }

    void move_from(const tfuture& other) {
        immediate = other.immediate;
        if (immediate) {
            val = std::move(other.val); 
        } else {
            fut = std::move(other.fut);
        }
    }

    void destroy() {
        if (immediate) {
            val.~T();
        } else {
            fut.~future();
        }
    }

    template <typename F, typename... TEnclosed>
    auto then(F f, TEnclosed&&... enclosed_vals) {

        using result_type = std::result_of_t<F(TEnclosed&...,T)>; 
        bool run_now = true;

        if (run_now && immediate) {
            return tfuture<result_type>(
                f(std::forward<TEnclosed>(enclosed_vals)..., val)
            );
        } else if (run_now && fut.fulfilled_here()) {
            return tfuture<result_type>(
                f(std::forward<TEnclosed>(enclosed_vals)..., fut.get())
            );
        }

        return tfuture<result_type>(fut.then(closure(
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

    T& unwrap() {
        return val;
    }

    T& get() {
        if (immediate) {
            return val;
        } else {
            return fut.get();
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

    bool run_now = true;
    if (run_now) {
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
    REQUIRE(tready(10).get() == 10);
}

TEST_CASE("Async immediate") {
    REQUIRE(tasync([] { return 10; }).get() == 10);
}

TEST_CASE("Then immediate") {
    REQUIRE(tready(10).then([] (int x) { return x * 2; }).get() == 20);
}

TEST_CASE("Then immediate with enclosed") {
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
        return tasync([idx] {
            return fib(idx - 1).then(
                [] (tfuture<int> a, int x) {
                    return a.then([x] (int y) { return x + y; });
                },
                fib(idx - 2)
            ).unwrap();
        }).unwrap();
    }
}
TEST_CASE("Fib") {
    auto ctx = launch_local(1);
    std::cout << int(fib(44).get()) << std::endl; 
}

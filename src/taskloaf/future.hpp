#pragma once

#include "data.hpp"
#include "untyped_future.hpp"

namespace taskloaf {

template <typename F, typename ValT, typename TupleT, size_t... I>
auto apply_args(F&& func, ValT& val, TupleT& args, std::index_sequence<I...>) {
    return func(std::get<I>(args)..., val);
}

template <typename F, typename TupleT, size_t... I>
auto apply_args(F&& func, TupleT& args, std::index_sequence<I...>) {
    return func(std::get<I>(args)...);
}

#if defined __GNUC__ || defined __llvm__
#define tl_likely(x) __builtin_expect((x), 1)
#define tl_unlikely(x) __builtin_expect((x), 0)
#else
#define tl_likely(x) (x)
#define tl_unlikely(x) (x)
#endif

bool can_run_here(const address& addr) {
    return addr == location::anywhere 
        || addr == location::here 
        || addr == cur_worker->get_addr();
}

template <typename T>
struct future {
    std::unique_ptr<untyped_future> fut;
    T val;

    future(): fut(nullptr), val() {}
    future(T val): fut(nullptr), val(std::move(val)) {}
    future(untyped_future fut):
        fut(std::make_unique<untyped_future>(std::move(fut))),
        val() 
    {}

    future(const future& other) {
        val = other.val;
        if (tl_unlikely(other.fut != nullptr)) {
            fut = std::make_unique<untyped_future>(*other.fut);
        }
    }

    future& operator=(const future& other) {
        val = other.val;
        if (tl_unlikely(other.fut != nullptr)) {
            fut = std::make_unique<untyped_future>(*other.fut);
        }
        return *this;
    }

    future(future&&) = default;
    future& operator=(future&&) = default;

    template <typename F, typename... TEnclosed>
    auto then(address where, F f, TEnclosed&&... enclosed_vals) {

        using result_type = std::result_of_t<F(TEnclosed&...,T&)>; 
        bool run_now = !cur_worker->needs_interrupt;
        bool immediately = !fut && run_now && can_run_here(where);
        if (tl_likely(immediately)) {
            return future<result_type>(
                f(enclosed_vals..., val)
            );

        } else if (!fut) {
            return future<result_type>(ut_task(where, closure(
                [] (std::tuple<data,std::tuple<std::decay_t<TEnclosed>...,T>>& p,_) {
                    return apply_args(
                        std::get<0>(p).template get<F>(),
                        std::get<sizeof...(TEnclosed)>(std::get<1>(p)),
                        std::get<1>(p), std::index_sequence_for<TEnclosed...>{}
                    );
                },
                std::make_tuple(
                    data(std::move(f)),
                    std::make_tuple(std::forward<TEnclosed>(enclosed_vals)...,val)
                )
            )));
        }

        // TODO: Is it worth checking for fut->is_fulfilled_here()?
        return future<result_type>(fut->then(where, closure(
            [] (std::tuple<data,std::tuple<std::decay_t<TEnclosed>...>>& p, data& d) {
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

    template <typename F, typename... TEnclosed,
        std::enable_if_t<!std::is_convertible<F,int>::value>* = nullptr>
    auto then(F f, TEnclosed&&... enclosed_vals) {
        return then(
            location::anywhere, std::move(f),
            std::forward<TEnclosed>(enclosed_vals)...
        );
    }

    T unwrap_delayed() {
        return T(fut->then([] (_,T& inner_fut) { 
            return (inner_fut.fut == nullptr) ? 
                ut_ready(inner_fut.val) :
                *inner_fut.fut; 
        }).unwrap());
    }

    T unwrap() & { (tl_likely(!fut)) ? val : unwrap_delayed(); }
    T unwrap() && { return (tl_likely(!fut)) ? std::move(val) : unwrap_delayed(); }

    T& get() & { return (!fut) ? val : fut->get().get<T>(); }
    T get() && { return (!fut) ? std::move(val) : fut->get().get<T>(); }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(fut == nullptr);
        if (fut == nullptr) {
            ar(val);
        } else {
            ar(*fut);
        }
    }

    void load(cereal::BinaryInputArchive& ar) {
        bool is_ready;
        ar(is_ready);
        if (is_ready) {
            ar(val);
        } else {
            fut = std::make_unique<untyped_future>();
            ar(*fut);
        }
    }
};

template <typename T>
auto ready(T&& val) {
    return future<std::decay_t<T>>(std::forward<T>(val));
}

template <typename F, typename... TEnclosed>
auto task(address where, F f, TEnclosed&&... enclosed_vals) {
    using result_type = std::result_of_t<F(TEnclosed&...)>;

    bool run_now = !cur_worker->needs_interrupt;
    if (tl_likely(run_now && can_run_here(where))) {
        return future<result_type>(f(enclosed_vals...));
    }
    return future<result_type>(ut_task(where, closure(
        [] (std::tuple<data,std::tuple<std::decay_t<TEnclosed>...>>& p,_) {
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

template <typename F, typename... TEnclosed,
    std::enable_if_t<!std::is_convertible<F,int>::value>* = nullptr>
auto task(F f, TEnclosed&&... enclosed_vals) {
    return task(
        location::anywhere, std::move(f),
        std::forward<TEnclosed>(enclosed_vals)...
    );
}

} //end namespace taskloaf

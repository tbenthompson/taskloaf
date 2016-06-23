

namespace taskloaf {

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
        if (unlikely(other.fut != nullptr)) {
            fut = std::make_unique<untyped_future>(*other.fut);
        }
    }

    future& operator=(const future& other) {
        val = other.val;
        if (unlikely(other.fut != nullptr)) {
            fut = std::make_unique<untyped_future>(*other.fut);
        }
        return *this;
    }


    template <typename F, typename... TEnclosed>
    auto then(F f, TEnclosed&&... enclosed_vals) {

        using result_type = std::result_of_t<F(TEnclosed...,T)>; 
        bool run_now = cur_worker->should_run_now();

        if (likely(!fut && run_now)) {
            return future<result_type>(
                f(std::forward<TEnclosed>(enclosed_vals)..., val)
            );
        } else if (!fut) {
            return future<result_type>(ut_async(closure(
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
        // TODO: Is it worth checking for fut->is_fulfilled_here()?
        return future<result_type>(fut->then(closure(
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
            return (inner_fut.fut == nullptr) ? 
                ut_ready(inner_fut.val) :
                *inner_fut.fut; 
        }).unwrap());
    }

    T unwrap() & { (likely(!fut)) ? val : unwrap_delayed(); }
    T unwrap() && { return (likely(!fut)) ? std::move(val) : unwrap_delayed(); }

    T& get() & { return (!fut) ? val : fut->get(); }
    T get() && { return (!fut) ? std::move(val) : fut->get().get<T>(); }

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

template <typename T>
auto ready(T&& val) {
    return future<std::decay_t<T>>(std::forward<T>(val));
}

template <typename F, typename... TEnclosed>
auto async(F f, TEnclosed&&... enclosed_vals) {
    using result_type = std::result_of_t<F(TEnclosed&...)>;

    bool run_now = cur_worker->should_run_now();
    if (likely(run_now)) {
        return future<result_type>(f());
    }
    return future<result_type>(ut_async(closure(
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

} //end namespace taskloaf

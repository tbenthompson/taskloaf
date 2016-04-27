#pragma once

#include "ivar.hpp"
#include "fnc.hpp"
#include "execute.hpp"
#include "closure.hpp"
#include "worker.hpp"

#include <cereal/types/tuple.hpp>
#include <cereal/types/memory.hpp>

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoidCall {};

template <typename Return, typename... Args>
struct PossiblyVoidCall<Return(Args...)> {
    template <typename F>
    static auto on(F&& f) {
        return std::make_tuple(f());
    }
};

template <typename... Args>
struct PossiblyVoidCall<void(Args...)> {
    template <typename F>
    static auto on(F&& f) {
        f();
        return std::make_tuple();
    }
};

template <typename... Ts>
struct MakeFuture {
    static auto on() {
        return Future<Ts...>(IVarRef{new_id()});
    }
};

template <typename... Ts>
auto make_future() {
    return MakeFuture<Ts...>::on();
}

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F&& fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    Function<get_signature<F>> fnc_container(std::forward<F>(fnc));
    decltype(make_future<Return>()) out_future = make_future<Return>();
    typedef decltype(out_future) OutT;
    typedef decltype(fnc_container) FncT;
    fut.add_trigger(
        [] (OutT out_future, FncT fnc_container, std::tuple<Ts...> val) {
            cur_worker->add_task(
                [] (OutT out, FncT fnc, std::tuple<Ts...> val) {
                    out.fulfill(PossiblyVoidCall<get_signature<F>>::on([&] () {
                        return apply_args(fnc, val);         
                    }));
                },
                std::move(out_future),
                std::move(fnc_container),
                val
            );
        },
        out_future, 
        std::move(fnc_container)
    );
    return out_future;
}

//TODO: unwrap doesn't actually need to do anything. It can be
//lazy and wait for usage.
template <typename T>
auto unwrap(const Future<T>& fut) {
    Future<typename T::type> out_future = make_future<typename T::type>();
    typedef decltype(out_future) OutT;
    fut.add_trigger(
        [] (OutT out_future, std::tuple<T> result) {
            std::get<0>(result).add_trigger(
                [] (OutT out_future, std::tuple<typename T::type> val) {
                    out_future.fulfill(val); 
                },
                std::move(out_future)
            );
        },
        out_future
    );
    return out_future;
}

template <typename T>
auto ready(T val) {
    auto out_future = make_future<T>();
    out_future.fulfill(std::make_tuple(std::move(val)));
    return out_future;
}

template <typename F>
auto async(F&& fnc) {
    typedef typename std::result_of<F()>::type Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    auto out_future = make_future<Return>();
    using FncT = decltype(fnc_container);
    using OutT = decltype(out_future);
    cur_worker->add_task(
        [] (OutT fut, FncT fnc) {
            fut.fulfill(PossiblyVoidCall<get_signature<F>>::on(fnc));
        },
        out_future,
        std::move(fnc_container)
    );
    return out_future;
}

// template <
//     typename F,
//     typename std::enable_if<
//         std::is_void<std::result_of_t<F()>>::value, int
//     >::type = 0
// >
// auto async(F&& fnc) {
//     return async(make_closure([] (F&& f) { f(); return 0; })
// }

template <size_t Idx, bool end, typename... Ts>
struct WhenAllHelper {
    static void run(std::tuple<Future<Ts>...> child_results, 
        std::tuple<Ts...> accum, Future<Ts...> result) 
    {
        typedef std::tuple<std::tuple_element_t<Idx,std::tuple<Ts...>>> ValT;

        auto next_result = std::get<Idx>(child_results);
        next_result.add_trigger(
            [] (std::tuple<Future<Ts>...> child_results,
                std::tuple<Ts...> accum, Future<Ts...> result, 
                ValT val) 
            {
                std::get<Idx>(accum) = std::get<0>(val);
                WhenAllHelper<Idx+1,Idx+2 == sizeof...(Ts),Ts...>::run(
                    std::move(child_results), std::move(accum),
                    std::move(result)
                );
            },
            std::move(child_results),
            std::move(accum),
            std::move(result) 
        );
    }
};

template <size_t Idx, typename... Ts>
struct WhenAllHelper<Idx, true, Ts...> {
    static void run(std::tuple<Future<Ts>...> child_results, 
        std::tuple<Ts...> accum, Future<Ts...> result) 
    {
        typedef std::tuple<std::tuple_element_t<Idx,std::tuple<Ts...>>> ValT;

        std::get<Idx>(child_results).add_trigger(
            [] (Future<Ts...> result, std::tuple<Ts...> accum, ValT val) 
            {
                std::get<Idx>(accum) = std::get<0>(val);
                result.fulfill(std::move(accum));
            },
            std::move(result),
            std::move(accum)
        );
    }
};

template <typename... FutureTs>
auto when_all(FutureTs&&... args) {
    auto data = std::make_tuple(std::forward<FutureTs>(args)...);
    auto out_future = make_future<
        typename std::decay<FutureTs>::type::type...
    >();
    WhenAllHelper<
        0, sizeof...(FutureTs) == 1,
        typename std::decay<FutureTs>::type::type...
    >::run(
        std::move(data), {}, out_future
    );
    return out_future;
}

template <typename... Ts>
struct FutureData {
    union Union {
        Union() {}
        ~Union() {}
        std::tuple<Ts...> val;
        IVarRef ivar;
    } d;
    bool ready;

    ~FutureData() {
        if (ready) {
            d.val.~tuple();
        } else {
            d.ivar.~IVarRef();
        }
    }
};

template <typename... Ts>
struct Future {
    using type = typename std::tuple_element<0,std::tuple<Ts...>>::type;

    std::shared_ptr<FutureData<Ts...>> data;

    Future(): data(std::make_shared<FutureData<Ts...>>()) {}

    Future(IVarRef&& ivar): Future() {
        data->ready = false;
        data->d.ivar = ivar;
    }

    Future(std::tuple<Ts...> val): Future() {
        data->ready = true;
        data->d.val = std::move(val);
    }

    template <typename F>
    auto then(F&& f) const {
        return taskloaf::then(*this, std::forward<F>(f));
    }

    auto unwrap() const {
        return taskloaf::unwrap(*this);
    }

    template <typename F, typename... Args>
    void add_trigger(F&& f, Args&&... args) const {
        if (data->ready) {
            f(std::forward<Args>(args)..., data->d.val);
        } else {
            auto trigger = make_closure(
                [f = std::forward<F>(f)] 
                (Args&... args, std::vector<Data>& vals) {
                    return f(args..., vals[0].get_as<std::tuple<Ts...>>());
                },
                std::forward<Args>(args)...
            );
            cur_worker->add_trigger(data->d.ivar, std::move(trigger));
        }
    }

    void fulfill(std::tuple<Ts...> val) const {
        if (data->ready) {
            data->d.val = std::move(val);
        } else {
            cur_worker->fulfill(data->d.ivar, {make_data(std::move(val))});
        }
    }

    template <typename Archive>
    void serialize(Archive& ar) const {
        ar(data->ready);
        if(data->ready) {
            ar(data->d.val);
        } else {
            ar(data->d.ivar);
        }
    }
};

template <>
struct FutureData<> {
    IVarRef ivar;
    bool ready;
};

template <>
struct Future<> {
    using type = void;

    std::shared_ptr<FutureData<>> data;

    Future(): data(std::make_shared<FutureData<>>()) {}

    Future(IVarRef&& ivar): Future() {
        data->ready = false;
        data->ivar = ivar;
    }

    Future(std::tuple<>): Future() {
        data->ready = true;
    }

    template <typename F>
    auto then(F&& f) const {
        return taskloaf::then(*this, std::forward<F>(f));
    }

    template <typename F, typename... Args>
    void add_trigger(F&& f, Args&&... args) const {
        if (data->ready) {
            f(std::forward<Args>(args)..., std::make_tuple());
        } else {
            auto trigger = make_closure(
                [f = std::forward<F>(f)] 
                (Args&... args, std::vector<Data>&) {
                    return f(args..., std::make_tuple());
                },
                std::forward<Args>(args)...
            );
            cur_worker->add_trigger(data->ivar, std::move(trigger));
        }
    }

    struct Empty {};

    void fulfill(std::tuple<>) const {
        if (data->ready) {
        } else {
            cur_worker->fulfill(data->ivar, {make_data(Empty())});
        }
    }

    template <typename Archive>
    void serialize(Archive& ar) const {
        ar(data->ready);
        if(!data->ready) {
            ar(data->ivar);
        }
    }
};

template <>
struct MakeFuture<void> {
    static auto on() {
        return Future<>(IVarRef{new_id()});
    }
};

} //end namespace taskloaf

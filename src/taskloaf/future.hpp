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

template <typename... Ts>
Future<Ts...> make_delayed_future() {
    return Future<Ts...>(IVarRef{new_id()});
}

template <typename... Ts>
Future<Ts...> make_ready_future(Ts&&... args) {
    return Future<Ts...>({make_data(std::forward<Ts>(args))...});
}

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F&& fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    Function<get_signature<F>> fnc_container(std::forward<F>(fnc));
    Future<Return> out_future = make_delayed_future<Return>();
    typedef decltype(out_future) OutT;
    typedef decltype(fnc_container) FncT;
    fut.add_trigger(
        [] (OutT out_future, FncT fnc_container, std::vector<Data>& vals) {
            cur_worker->add_task(
                [] (OutT out, FncT fnc, std::vector<Data> vals) {
                    out.fulfill({make_data(apply_args(fnc, vals))});
                },
                std::move(out_future),
                std::move(fnc_container),
                vals
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
    Future<typename T::type> out_future = make_delayed_future<typename T::type>();
    typedef decltype(out_future) OutT;
    fut.add_trigger(
        [] (OutT out_future, std::vector<Data>& vals) {
            auto result = vals[0].get_as<T>();
            result.add_trigger(
                [] (OutT out_future, std::vector<Data>& vals) {
                    out_future.fulfill(vals); 
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
    auto out_future = make_delayed_future<T>();
    out_future.fulfill({make_data(std::move(val))});
    return out_future;
}

template <typename F>
auto async(F&& fnc) {
    typedef typename std::result_of<F()>::type Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    auto out_future = make_delayed_future<Return>();
    using FncT = decltype(fnc_container);
    using OutT = decltype(out_future);
    cur_worker->add_task(
        [] (OutT fut, FncT fnc) {
            fut.fulfill({make_data(fnc())});
        },
        out_future,
        std::move(fnc_container)
    );
    return out_future;
}

template <size_t Idx, bool end, typename... Ts>
struct WhenAllHelper {
    static void run(std::tuple<Future<Ts>...> child_results, 
        std::vector<Data> accum, Future<Ts...> result) 
    {
        auto next_result = std::get<Idx>(child_results);
        next_result.add_trigger(
            [] (std::tuple<Future<Ts>...> child_results,
                std::vector<Data> accum, Future<Ts...> result, 
                std::vector<Data>& vals) 
            {
                accum.push_back(vals[0]);
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
        std::vector<Data> accum, Future<Ts...> result) 
    {
        std::get<Idx>(child_results).add_trigger(
            [] (Future<Ts...> result, std::vector<Data> accum,
                std::vector<Data>& vals) 
            {
                accum.push_back(vals[0]);
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
    auto out_future = make_delayed_future<
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

struct FutureData {
    union Union {
        Union() {}
        ~Union() {}
        std::vector<Data> vals;
        IVarRef ivar;
    } d;
    bool ready;

    ~FutureData() {
        if (ready) {
            d.vals.~vector<Data>();
        } else {
            d.ivar.~IVarRef();
        }
    }
};

template <typename... Ts>
struct Future {
    using type = typename std::tuple_element<0,std::tuple<Ts...>>::type;

    std::shared_ptr<FutureData> data;

    Future(): data(std::make_shared<FutureData>()) {}

    Future(IVarRef&& ivar): Future() {
        data->ready = false;
        data->d.ivar = ivar;
    }

    Future(std::vector<Data> vals): Future() {
        data->ready = true;
        data->d.vals = std::move(vals);
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
        auto trigger = make_closure(
            std::forward<F>(f), std::forward<Args>(args)...
        );

        if (data->ready) {
            trigger(data->d.vals);
        } else {
            cur_worker->add_trigger(data->d.ivar, std::move(trigger));
        }
    }

    void fulfill(std::vector<Data> vals) const {
        if (data->ready) {
            data->d.vals = std::move(vals);
        } else {
            cur_worker->fulfill(data->d.ivar, std::move(vals));
        }
    }

    template <typename Archive>
    void serialize(Archive& ar) const {
        ar(data->ready);
        if(data->ready) {
            ar(data->d.vals);
        } else {
            ar(data->d.ivar);
        }
    }
};

} //end namespace taskloaf

#pragma once

#include "ivar.hpp"
#include "fnc.hpp"
#include "execute.hpp"
#include "closure.hpp"
#include "worker.hpp"

#include <cereal/types/tuple.hpp>
#include <cereal/types/memory.hpp>

namespace taskloaf {

template <typename... Ts> auto make_delayed_future();
template <typename... Ts> auto make_ready_future(Ts&&... args);
template <typename... Ts> struct Future;

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F&& fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    auto out_future = make_delayed_future<Return>();
    fut.add_trigger(
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            cur_worker->add_task({
                [] (std::vector<Data>& d) {
                    auto& out = d[0].get_as<decltype(out_future)>();
                    auto& fnc = d[1].get_as<decltype(fnc_container)>();
                    auto& vals = d[2].get_as<std::vector<Data>>();
                    out.fulfill({
                        make_data(apply_args(fnc, vals))
                    });
                },
                {d[0], d[1], make_data(vals)}
            });
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
    auto out_future = make_delayed_future<typename T::type>();
    fut.add_trigger(
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            auto result = vals[0].get_as<T>();
            result.add_trigger(
                [] (std::vector<Data>& d, std::vector<Data>& vals) {
                    d[0].get_as<decltype(out_future)>().fulfill(vals); 
                },
                d[0].get_as<decltype(out_future)>()
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
    cur_worker->add_task(
        [] (std::vector<Data>& d) {
            auto& fut = d[0].get_as<decltype(out_future)>();
            auto& fnc = d[1].get_as<decltype(fnc_container)>(); 
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
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& child_results = d[0].get_as<std::tuple<Future<Ts>...>>();
                auto& accum = d[1].get_as<std::vector<Data>>();
                auto& result = d[2].get_as<Future<Ts...>>();
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
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& result = d[0].get_as<Future<Ts...>>();
                auto& accum = d[1].get_as<std::vector<Data>>();
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
        TriggerT trigger{
            std::forward<F>(f),
            {make_data(std::forward<Args>(args))...}
        };
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

template <typename... Ts>
auto make_delayed_future() {
    return Future<Ts...>(IVarRef{new_id()});
}

template <typename... Ts>
auto make_ready_future(Ts&&... args) {
    return Future<Ts...>({make_data(std::forward<Ts>(args))...});
}

} //end namespace taskloaf

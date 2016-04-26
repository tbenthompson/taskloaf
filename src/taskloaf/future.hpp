#pragma once

#include "ivar.hpp"
#include "fnc.hpp"
#include "execute.hpp"
#include "closure.hpp"
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F, typename... Ts>
auto then(const Future<Ts...>& fut, F&& fnc) {
    typedef typename std::result_of<F(Ts&...)>::type Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    IVarRef out_future(new_id());
    cur_worker->add_trigger(fut.ivar, {
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            cur_worker->add_task({
                [] (std::vector<Data>& d) {
                    auto& out_future = d[0].get_as<IVarRef>();
                    auto& fnc = d[1].get_as<decltype(fnc_container)>();
                    auto& vals = d[2].get_as<std::vector<Data>>();
                    cur_worker->fulfill(out_future, {
                        make_data(apply_args(vals, fnc))
                    });
                },
                {d[0], d[1], make_data(vals)}
            });
        },
        {make_data(out_future), make_data(std::move(fnc_container))}
    });
    return Future<Return>{out_future};
}

//TODO: unwrap doesn't actually need to do anything. It can be
//lazy and wait for usage.
template <typename T>
auto unwrap(const Future<T>& fut) {
    IVarRef out_future(new_id());
    cur_worker->add_trigger(fut.ivar, {
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            auto result = vals[0].get_as<T>().ivar;
            cur_worker->add_trigger(result, {
                [] (std::vector<Data>& d, std::vector<Data>& vals) {
                    cur_worker->fulfill(d[0].get_as<IVarRef>(), vals); 
                },
                {d[0]}
            });
        },
        {make_data(out_future)}
    });
    return Future<typename T::type>{out_future};
}

template <typename T>
auto ready(T val) {
    IVarRef out_future(new_id());
    cur_worker->fulfill(out_future, {make_data(std::move(val))});
    return Future<T>{out_future};
}

template <typename F>
auto async(F&& fnc) {
    typedef typename std::result_of<F()>::type Return;
    auto fnc_container = make_function(std::forward<F>(fnc));
    IVarRef out_future(new_id());
    cur_worker->add_task({
        [] (std::vector<Data>& d) {
            auto& out_future = d[0].get_as<IVarRef>();
            auto& fnc = d[1].get_as<decltype(fnc_container)>(); 
            cur_worker->fulfill(out_future, {make_data(fnc())});
        },
        {make_data(out_future), make_data(std::move(fnc_container))}
    });
    return Future<Return>{out_future};
}

inline void whenall_child(std::vector<IVarRef> child_results, size_t next_idx,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto next_result = std::move(child_results[next_idx]);
    if (next_idx == child_results.size() - 1) {
        cur_worker->add_trigger(next_result, {
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& result = d[0].get_as<IVarRef>();
                auto& accumulator = d[1].get_as<std::vector<Data>>();
                accumulator.push_back(vals[0]);
                cur_worker->fulfill(result, std::move(accumulator));
            },
            { make_data(result), make_data(std::move(accumulator)) }
        });
    } else {
        cur_worker->add_trigger(next_result, {
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& child_results = d[0].get_as<std::vector<IVarRef>>();
                auto& next_idx = d[1].get_as<size_t>();
                auto& accumulator = d[2].get_as<std::vector<Data>>();
                auto& result = d[3].get_as<IVarRef>();
                accumulator.push_back(vals[0]);
                whenall_child(std::move(child_results), next_idx + 1,
                    std::move(accumulator), std::move(result)
                );
            },
            {
                make_data(std::move(child_results)), make_data(next_idx),
                make_data(std::move(accumulator)), make_data(std::move(result)) 
            }
        });
    }
}

IVarRef plan_when_all(std::vector<IVarRef> inputs) {
    IVarRef out_future(new_id());
    whenall_child(std::move(inputs), 0, {}, out_future);
    return out_future;
}

template <typename... Ts>
auto when_all(const Future<Ts>&... args) {
    std::vector<IVarRef> data{args.ivar...};
    return Future<Ts...>{plan_when_all(std::move(data))};
}


template <typename... Ts>
struct Future {
    IVarRef ivar;

    template <typename F>
    auto then(F&& f) const {
        return taskloaf::then(*this, std::forward<F>(f));
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ivar);
    }
};

template <typename T>
struct Future<T> {
    using type = T;

    IVarRef ivar;

    template <typename F>
    auto then(F&& f) const {
        return taskloaf::then(*this, std::forward<F>(f));
    }
    
    auto unwrap() const {
        return taskloaf::unwrap(*this);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ivar);
    }
};

} //end namespace taskloaf

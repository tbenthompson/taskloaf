#pragma once
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoid{};

template <typename Return, typename... Args>
struct PossiblyVoid<Return(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        return std::vector<Data>{make_data(f())};
    }
};

template <typename... Args>
struct PossiblyVoid<void(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        f();
        return std::vector<Data>{};
    }
};

template <typename F>
auto possibly_void(const F& f) {
    return PossiblyVoid<GetSignature<F>>::on(f);
}

struct InternalLoc {
    bool anywhere;
    Address where;

    template <typename Ar> 
    void serialize(Ar& ar) { ar(anywhere); ar(where); }
};

InternalLoc internal_loc(Loc loc) {
    if (loc == Loc::here) {
        return {false, cur_worker->get_addr()};
    } else {
        return {true, {}};
    }
}

void schedule_task(const InternalLoc& iloc, TaskT t) {
    auto& task_collection = cur_worker->get_task_collection();
    if (iloc.anywhere) {
        task_collection.add_task(std::move(t));
    } else {
        task_collection.add_task(iloc.where, std::move(t));
    }
}

template <typename F, typename... Ts>
auto then(Loc loc, Future<Ts...>& fut, F&& fnc) {
    typedef std::result_of_t<F(Ts&...)> Return;

    Future<Return> out_future;
    auto f_serializable = make_function(std::forward<F>(fnc));
    auto iloc = internal_loc(loc);
    fut.add_trigger(TriggerT{
        [] (std::vector<Data>& c_args, std::vector<Data>& args) {
            TaskT t{
                [] (std::vector<Data>& args)  {
                    auto& out_future = args[1].get_as<Future<Return>>();
                    out_future.fulfill(possibly_void(
                        [&] () {
                            return apply_data_args(
                                args[0].get_as<decltype(f_serializable)>(),
                                args[2].get_as<std::vector<Data>>()
                            );
                        }
                    ));
                },
                {c_args[0], c_args[1], make_data(args)}
            };
            auto iloc = c_args[2].get_as<InternalLoc>();
            schedule_task(iloc, std::move(t));
        },
        {
            make_data(std::move(f_serializable)),
            make_data(out_future),
            make_data(iloc)
        }
    });
    return out_future;
}

template <typename Fut>
auto unwrap(Fut&& fut) {
    typedef typename std::decay_t<Fut>::type T;
    typedef Future<typename T::type> FutT;

    FutT out_future;
    fut.add_trigger(TriggerT{
        [] (std::vector<Data>& c_args, std::vector<Data>& args) {
            T& inner_fut = args[0].get_as<T>();
            inner_fut.add_trigger(TriggerT{
                [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                    c_args[0].get_as<FutT>().fulfill(args);
                },
                {c_args[0]}
            });
        },
        {make_data(out_future)}
    });
    return out_future;
}

template <typename T>
auto ready(T&& val) {
    Future<std::decay_t<T>> out_future;
    out_future.fulfill({make_data(std::forward<T>(val))});
    return out_future;
}

template <typename F>
auto async(Loc loc, F&& fnc) {
    typedef std::result_of_t<F()> Return;
    Future<Return> out_future;
    auto f_serializable = make_function(fnc);
    auto iloc = internal_loc(loc);
    TaskT t{
        [] (std::vector<Data>& args) {
            args[1].get_as<Future<Return>>().fulfill(possibly_void(
                [&] () {
                    return args[0].get_as<decltype(f_serializable)>()();
                }
            ));
        },
        {
            make_data(f_serializable),
            make_data(out_future)
        }
    };
    schedule_task(iloc, std::move(t));
    return out_future;
}

template <typename F>
auto async(F&& fnc) {
    return async(Loc::anywhere, std::forward<F>(fnc));
}

} //end namespace taskloaf

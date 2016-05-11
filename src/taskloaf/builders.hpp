#pragma once
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoidCall {};

template <typename Return, typename... Args>
struct PossiblyVoidCall<Return(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        return std::make_tuple(f());
    }
};

template <typename... Args>
struct PossiblyVoidCall<void(Args...)> {
    template <typename F>
    static auto on(const F& f) {
        f();
        return std::make_tuple();
    }
};

template <typename F>
auto possibly_void_call(const F& f) {
    return PossiblyVoidCall<GetSignature<F>>::on(f);
}

template <typename F, typename... Ts>
auto then(Future<Ts...>& fut, F fnc) {
    typedef decltype(apply_args(fnc, fut.get_val())) Return;
    Future<Return> out_future;
    
    bool immediately = fut.can_trigger_immediately() && can_run_immediately();

    if (immediately) {
        out_future.fulfill(possibly_void_call([&] () {
            return apply_args(fnc, fut.get_val());
        }));
    } else {
        auto f_serializable = make_function(std::forward<F>(fnc));
        fut.add_trigger(TriggerT{
            [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                cur_worker->add_task(TaskT{
                    [] (std::vector<Data>& args)  {
                        auto& out_future = args[1].get_as<Future<Return>>();
                        out_future.fulfill(possibly_void_call([&] () {
                            return apply_args(
                                args[0].get_as<decltype(f_serializable)>(),
                                args[2].get_as<std::tuple<Ts...>>()
                            );
                        }));
                    },
                    {c_args[0], c_args[1], args[0]}
                }, false);
            },
            {
                make_data(std::move(f_serializable)),
                make_data(out_future)
            }
        });
    }
    return out_future;
}

template <typename T>
auto unwrap(Future<T>& fut) {

    typedef Future<typename T::type> FutT;
    FutT out_future;

    bool immediately = fut.can_trigger_immediately() &&
        std::get<0>(fut.get_val()).can_trigger_immediately();

    if (immediately) {
        out_future.fulfill(std::get<0>(fut.get_val()).get_val());     
    } else {
        fut.add_trigger(TriggerT{
            [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                T& inner_fut = std::get<0>(args[0].get_as<std::tuple<T>>());
                inner_fut.add_trigger(TriggerT{
                    [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                        c_args[0].get_as<FutT>().fulfill(
                            args[0].get_as<std::tuple<typename T::type>>()
                        );
                    },
                    {c_args[0]}
                });
            },
            {make_data(out_future)}
        });
    }
    return out_future;
}

template <typename T>
auto ready(T val) {
    Future<T> out_future;
    out_future.fulfill(std::make_tuple(std::move(val)));
    return out_future;
}

const static bool push = true;

template <typename F>
auto async(F fnc, bool push = false) {
    typedef typename std::result_of<F()>::type Return;

    Future<Return> out_future;

    if (can_run_immediately() && !push) {
        out_future.fulfill(possibly_void_call(fnc));
    } else {
        auto f_serializable = make_function(fnc);
        cur_worker->add_task(TaskT{
            [] (std::vector<Data>& args) {
                args[1].get_as<Future<Return>>().fulfill(possibly_void_call([&] () {
                    return args[0].get_as<decltype(f_serializable)>()();
                }));
            },
            {
                make_data(f_serializable),
                make_data(out_future)
            }
        }, push);
    }
    return out_future;
}

} //end namespace taskloaf

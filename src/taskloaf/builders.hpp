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

template <typename Fut, typename F>
auto then(Fut&& fut, F&& fnc) {
    typedef typename std::decay<Fut>::type DecayFut;
    typedef decltype(apply_args(fnc, fut.get())) Return;
    
    bool immediately = fut.can_trigger_immediately() && \
        can_run_immediately(fut.owner);
    if (immediately) {
        return Future<Return>(fut.owner, possibly_void_call([&] () {
            return apply_args(std::forward<F>(fnc), fut.get());
        }));
    } else {
        Future<Return> out_future;
        auto f_serializable = make_function(std::forward<F>(fnc));
        fut.add_trigger(TriggerT{
            [] (std::vector<Data>& c_args, std::vector<Data>& args) {
                cur_worker->add_task(TaskT{
                    [] (std::vector<Data>& args)  {
                        auto& out_future = args[1].get_as<Future<Return>>();
                        out_future.fulfill(possibly_void_call([&] () {
                            return apply_args(
                                args[0].get_as<decltype(f_serializable)>(),
                                args[2].get_as<typename DecayFut::TupleT>()
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
        return out_future;
    }
}

template <typename Fut>
auto unwrap(Fut&& fut) {
    typedef typename std::decay<Fut>::type::type T;
    typedef Future<typename T::type> FutT;

    bool immediately = fut.can_trigger_immediately() &&
        std::get<0>(fut.get()).can_trigger_immediately();

    if (immediately) {
        return FutT(fut.owner, std::get<0>(std::forward<Fut>(fut).get()).get());     
    } else {
        FutT out_future;
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
        return out_future;
    }
}

template <typename T>
auto ready(T val) {
    return Future<T>(cur_worker, std::make_tuple(std::move(val)));
}

const static bool push = true;

template <typename F>
auto async(F&& fnc, bool push = false) {
    typedef typename std::result_of<F()>::type Return;

    auto* cw = cur_worker;
    if (can_run_immediately(cw) && !push) {
        return Future<Return>(cw, possibly_void_call(std::forward<F>(fnc)));
    } else {
        Future<Return> out_future;
        auto f_serializable = make_function(fnc);
        cw->add_task(TaskT{
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
        return out_future;
    }
}

} //end namespace taskloaf

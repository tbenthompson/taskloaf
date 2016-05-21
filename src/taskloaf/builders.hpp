#pragma once
#include "worker.hpp"

namespace taskloaf {

template <typename... Ts> struct Future;

template <typename F>
struct PossiblyVoidCall {};

template <typename Return, typename... Args>
struct PossiblyVoidCall<Return(Args...)> {
    template <typename OutF, typename F>
    static auto on(const OutF& out_f, const F& f) {
        return out_f(f());
    }
};

template <typename... Args>
struct PossiblyVoidCall<void(Args...)> {
    template <typename OutF, typename F>
    static auto on(const OutF& out_f, const F& f) {
        f();
        return out_f();
    }
};

template <typename OutF, typename F>
auto possibly_void_call(const OutF& out_f, const F& f) {
    return PossiblyVoidCall<GetSignature<F>>::on(out_f, f);
}

template <typename F>
auto possibly_void_tuple(const F& f) {
    return possibly_void_call([] (auto... v) { return std::make_tuple(
        std::forward<decltype(v)>(v)...); 
    }, f);
}

template <typename F>
auto possibly_void_data(const F& f) {
    return possibly_void_call([] (auto... v) {
        return std::vector<Data>{make_data(std::forward<decltype(v)>(v))...}; 
    }, f);
}

template <typename Fut, typename F>
auto thend(Fut&& fut, F&& fnc) {
    typedef decltype(apply_args(fnc, fut.get())) Return;

    Future<Return> out_future;
    auto f_serializable = make_function(std::forward<F>(fnc));
    fut.add_trigger(TriggerT{
        [] (std::vector<Data>& c_args, std::vector<Data>& args) {
            cur_worker->add_task(TaskT{
                [] (std::vector<Data>& args)  {
                    auto& out_future = args[1].get_as<Future<Return>>();
                    out_future.fulfill(possibly_void_data(
                        [&] () {
                            return apply_data_args(
                                args[0].get_as<decltype(f_serializable)>(),
                                args[2].get_as<std::vector<Data>>()
                            );
                        }
                    ));
                },
                {c_args[0], c_args[1], make_data(args)}
            });
        },
        {
            make_data(std::move(f_serializable)),
            make_data(out_future)
        }
    });
    return out_future;
}

template <typename Fut, typename F>
auto then(Fut&& fut, F&& fnc) {
    typedef decltype(apply_args(fnc, fut.get())) Return;
    
    bool immediately = fut.can_trigger_immediately() && \
        can_run_immediately(fut.owner);
    if (immediately) {
        return Future<Return>(fut.owner, possibly_void_tuple(
            [&] () { return apply_args(std::forward<F>(fnc), fut.get()); }
        ));
    } else {
        return thend(std::forward<Fut>(fut), std::forward<F>(fnc));
    }
}

template <typename Fut>
auto unwrap(Fut&& fut) {
    typedef typename std::decay_t<Fut>::type T;
    typedef Future<typename T::type> FutT;

    bool immediately = fut.can_trigger_immediately() &&
        std::get<0>(fut.get()).can_trigger_immediately();

    if (immediately) {
        return FutT(fut.owner, std::get<0>(std::forward<Fut>(fut).get()).get());     
    } else {
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
}

template <typename T>
auto ready(T val) {
    return Future<T>(cur_worker, std::make_tuple(std::move(val)));
}

const static bool push = true;

template <typename F>
auto asyncd(F&& fnc) {
    typedef std::result_of_t<F()> Return;
    Future<Return> out_future;
    auto f_serializable = make_function(fnc);
    cur_worker->add_task(TaskT{
        [] (std::vector<Data>& args) {
            args[1].get_as<Future<Return>>().fulfill(possibly_void_data(
                [&] () {
                    return args[0].get_as<decltype(f_serializable)>()();
                }
            ));
        },
        {
            make_data(f_serializable),
            make_data(out_future)
        }
    });
    return out_future;
}

template <typename F>
auto async(F&& fnc) {
    typedef std::result_of_t<F()> Return;

    auto* cw = cur_worker;
    if (can_run_immediately(cw)) {
        return Future<Return>(cw, possibly_void_tuple(std::forward<F>(fnc)));
    } else {
        return asyncd(std::forward<F>(fnc));
    }
}

} //end namespace taskloaf

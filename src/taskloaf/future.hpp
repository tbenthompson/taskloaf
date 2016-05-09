#pragma once

#include "builders.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename... Ts>
struct FutureData {
    union Union {
        Union() {}
        ~Union() {}
        std::tuple<Ts...> val;
        IVarRef ivar;
    } d;
    bool local;
    bool fulfilled;

    FutureData(bool local):
        local(local),
        fulfilled(false)
    {
        if (local) {
            new (&d.val) std::tuple<Ts...>();
        } else {
            new (&d.ivar) IVarRef();
        }
    }

    ~FutureData() {
        if (local) {
            d.val.~tuple();
        } else {
            d.ivar.~IVarRef();
        }
    }

    std::tuple<Ts...>& get_val() { return d.val; }
    const std::tuple<Ts...>& get_val() const { return d.val; }
    IVarRef& get_ivar() { return d.ivar; }
    const IVarRef& get_ivar() const { return d.ivar; }
};

template <>
struct FutureData<> {
    IVarRef ivar;
    bool local;
    bool fulfilled;

    FutureData(bool local):
        local(local),
        fulfilled(false)
    {}

    static std::tuple<> empty;

    std::tuple<>& get_val() { return empty; }
    const std::tuple<>& get_val() const { return empty; }
    IVarRef& get_ivar() { return ivar; }
    const IVarRef& get_ivar() const { return ivar; }
};

template <typename Derived, typename... Ts>
struct FutureBase {
    std::shared_ptr<FutureData<Ts...>> data;

    FutureBase():
        data(std::make_shared<FutureData<Ts...>>(true))
    {}

    FutureBase(IVarRef&& ivar): 
        data(std::make_shared<FutureData<Ts...>>(false)) 
    {
        data->get_ivar() = std::move(ivar);
    }

    auto& derived() {
        return *static_cast<Derived*>(this);
    }

    template <typename F>
    auto then(F&& f) {
        return taskloaf::then(derived(), std::forward<F>(f));
    }

    template <typename F, typename... Args>
    void add_trigger(F&& f, Args&&... args) {
        if (data->local && data->fulfilled && can_run_immediately()) {
            f(args..., data->get_val());
        } else {
            cur_worker->add_trigger(data->get_ivar(), TriggerT{
                [] (std::vector<Data>& args, std::vector<Data>& trigger_args) {
                    //TODO: Remove this copying
                    std::vector<Data> non_fnc_args;
                    for (size_t i = 1; i < args.size(); i++) {
                        non_fnc_args.push_back(args[i]);
                    }
                    for (size_t i = 1; i < trigger_args.size(); i++) {
                        non_fnc_args.push_back(trigger_args[i]);
                    }
                    apply_data_args(
                        args[0].get_as<typename std::decay<F>::type>(),
                        non_fnc_args
                    );
                },
                std::vector<Data>{ 
                    make_data(std::forward<F>(f)),
                    make_data(std::forward<Args>(args))...
                }
            });
        }
    }

    void fulfill(std::tuple<Ts...> val) {
        data->fulfilled = true;
        if (data->local) {
            data->get_val() = std::move(val);
        } else {
            cur_worker->fulfill(data->get_ivar(), {make_data(std::move(val))});
        }
    }


    template <typename Archive>
    void serialize(Archive& ar) const {
        ar(data->local);
        if(data->local) {
            if (data->fulfilled) {
                ar(data->get_val());
            }
        } else {
            ar(data->get_ivar());
        }
    }
};

template <typename... Ts>
struct Future: public FutureBase<Future<Ts...>,Ts...> {
    using type = typename std::tuple_element<0,std::tuple<Ts...>>::type;

    using FutureBase<Future<Ts...>,Ts...>::FutureBase;

    auto unwrap() {
        return taskloaf::unwrap(*this);
    }
};

template <>
struct Future<>: public FutureBase<Future<>> {
    using type = void;

    using FutureBase<Future<>>::FutureBase;
};

template <>
struct Future<void>: Future<> {};

} //end namespace taskloaf

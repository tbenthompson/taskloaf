#pragma once

#include "builders.hpp"
#include "ivar.hpp"

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
        data(std::make_shared<FutureData<Ts...>>())
    {
        data->local = true;
        data->fulfilled = false;
    }

    FutureBase(IVarRef&& ivar): FutureBase() {
        data->local = false;
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
        if (data->local 
            && data->fulfilled 
            && cur_worker->can_compute_immediately()) 
        {
            f(args..., data->get_val());
        } else {
            auto trigger = make_closure(
                [f = std::forward<F>(f)] 
                (Args&... args, std::vector<Data>& vals) {
                    return f(args..., vals[0].get_as<std::tuple<Ts...>>());
                },
                std::forward<Args>(args)...
            );
            cur_worker->add_trigger(data->get_ivar(), std::move(trigger));
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

template <typename... Ts>
struct MakeFuture {
    static auto on() {
        auto out = Future<Ts...>();
        out.data->local = true;
        // out.data->get_ivar() = IVarRef{new_id()};
        return out;
    }
};

template <>
struct MakeFuture<void> {
    static auto on() {
        auto out = Future<>();
        out.data->local = true;
        // out.data->get_ivar() = IVarRef{new_id()};
        return out;
    }
};

template <typename... Ts>
auto make_future() {
    return MakeFuture<Ts...>::on();
}

} //end namespace taskloaf

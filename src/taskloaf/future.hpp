#pragma once

#include "builders.hpp"
#include "when_all.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename... Ts>
struct FutureData {
    std::tuple<Ts...> val;
    bool local = true;
    bool fulfilled = false;
};

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;

    std::shared_ptr<FutureData<Ts...>> data;
    IVarRef ivar;

    FutureBase(): data(std::make_shared<FutureData<Ts...>>()) { 
        make_global();
    }
    FutureBase(FutureBase&& other) = default;
    FutureBase& operator=(FutureBase&& other) = default;

    FutureBase(const FutureBase& other) {
        *this = other; 
    }
    FutureBase& operator=(const FutureBase& other) {
        data = other.data;
        make_global();
        return *this;
    }

    void make_global() {
        if (!data->local) {
            return;
        }
        data->local = false;
        ivar = IVarRef(new_id()); 
        if (data->fulfilled) {
            fulfill(std::move(data->val));
        }
    }

    template <typename F>
    auto then(F&& f) {
        return taskloaf::then(*static_cast<Derived*>(this), std::forward<F>(f));
    }

    bool can_trigger_immediately() {
        return data->local && data->fulfilled;
    }

    void add_trigger(TriggerT trigger) {
        make_global();
        cur_worker->add_trigger(ivar, std::move(trigger));
    }

    void fulfill(std::tuple<Ts...> val) {
        data->fulfilled = true;
        if (data->local) {
            data->val = std::move(val);
        } else {
            cur_worker->fulfill(ivar, {make_data(std::move(val))});
        }
    }


    template <typename Archive>
    void serialize(Archive& ar) {
        ar(data->local);
        if(data->local) {
            if (data->fulfilled) {
                ar(data->val);
            }
        } else {
            ar(ivar);
        }
    }
};

template <typename... Ts>
struct Future: public FutureBase<Future<Ts...>,Ts...> {
    using FutureBase<Future<Ts...>,Ts...>::FutureBase;
};

template <typename T>
struct Future<T>: public FutureBase<Future<T>,T> {
    using type = T;

    auto unwrap() {
        return taskloaf::unwrap(*this);
    }
};

template <>
struct Future<>: public FutureBase<Future<>> {
    using FutureBase<Future<>>::FutureBase;

    using type = void;
};

template <>
struct Future<void>: Future<> {};

} //end namespace taskloaf

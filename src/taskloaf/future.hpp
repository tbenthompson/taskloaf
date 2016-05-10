#pragma once

#include "builders.hpp"
#include "when_all.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename... Ts>
struct FutureData {
};

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;

    mutable bool local = true;
    mutable bool fulfilled = false;
    mutable TupleT data;
    mutable IVarRef ivar;

    FutureBase() = default;
    FutureBase(FutureBase&& other) = default;
    FutureBase& operator=(FutureBase&& other) = default;

    FutureBase(const FutureBase& other) {
        *this = other; 
    }
    FutureBase& operator=(const FutureBase& other) {
        other.make_global();
        local = false;
        ivar = other.ivar;
        return *this;
    }

    void make_global() const {
        if (!local) {
            return;
        }
        local = false;
        ivar = IVarRef(new_id()); 
        if (is_fulfilled()) {
            fulfill(std::move(get_val()));
        }
    }

    bool is_fulfilled() const {
        return fulfilled;
    }

    TupleT& get_val() const {
        return data;
    }

    template <typename F>
    auto then(F&& f) {
        return taskloaf::then(*static_cast<Derived*>(this), std::forward<F>(f));
    }

    bool can_trigger_immediately() {
        return local && is_fulfilled();
    }

    void add_trigger(TriggerT trigger) {
        make_global();
        cur_worker->add_trigger(ivar, std::move(trigger));
    }

    void fulfill(std::tuple<Ts...> val) const {
        if (local) {
            fulfilled = true;
            data = std::move(val);
        } else {
            cur_worker->fulfill(ivar, {make_data(std::move(val))});
        }
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        make_global();
        ar(ivar);
    }

    void load(cereal::BinaryInputArchive& ar) {
        local = false;
        ar(ivar);
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

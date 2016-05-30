#pragma once

#include "builders.hpp"
#include "when_all.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename Tuple, size_t... I>
auto tuple_to_data_vector(Tuple&& t, std::index_sequence<I...>) {
    return std::vector<Data>{make_data(std::get<I>(std::forward<Tuple>(t)))...};
}

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;
    static std::index_sequence_for<Ts...> idxs;

    Worker* owner;
    mutable TupleT val;
    mutable std::unique_ptr<IVarRef> ivar = nullptr;

    FutureBase():
        owner(cur_worker), ivar(std::make_unique<IVarRef>(new_id())) 
    {}

    FutureBase(Worker* owner): 
        owner(owner), ivar(std::make_unique<IVarRef>(new_id())) 
    {}

    template <typename T,
        typename std::enable_if<
            std::is_same<typename std::decay<T>::type, TupleT>::value,void
        >::type* = nullptr
    >
    explicit FutureBase(Worker* owner, T&& val):
        owner(owner), val(std::move(val)) 
    {}

    FutureBase(FutureBase&& other) = default;
    FutureBase(const FutureBase& other) {
        *this = other;
    }

    FutureBase& operator=(FutureBase&& other) = default;
    FutureBase& operator=(const FutureBase& other) {
        owner = other.owner;
        other.ensure_at_least_global();
        ivar = std::make_unique<IVarRef>(*other.ivar);
        return *this;
    }

    bool is_global() const { return ivar != nullptr; }

    bool is_local() const { return !is_global(); }

    bool can_trigger_immediately() const {
        return !is_global() || owner->get_ivar_tracker().is_fulfilled_here(*ivar);
    }

    auto& get_tuple() & { return val; }
    auto&& get_tuple() && { return std::move(val); }

    void add_trigger(TriggerT trigger) const {
        ensure_at_least_global();
        owner->get_ivar_tracker().add_trigger(*ivar, std::move(trigger));
    }

    void fulfill(std::vector<Data> vals) const {
        owner->get_ivar_tracker().fulfill(*ivar, vals);
    }

    void ensure_at_least_global() const {
        if (ivar != nullptr) {
            return;
        }
        ivar = std::make_unique<IVarRef>(new_id());
        fulfill(tuple_to_data_vector(std::move(val), idxs));
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ensure_at_least_global();
        ar(*ivar);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ivar = std::make_unique<IVarRef>();
        owner = cur_worker;
        ar(*ivar);
    }

    Derived& derived() {
        return *static_cast<Derived*>(this);
    }

    template <typename F>
    auto then(F&& f) & {
        return taskloaf::then(derived(), std::forward<F>(f));
    }

    template <typename F>
    auto then(F&& f) && {
        return taskloaf::then(std::move(derived()), std::forward<F>(f));
    }

    template <typename F>
    auto thend(F&& f) & {
        return taskloaf::thend(derived(), std::forward<F>(f));
    }

    template <typename F>
    auto thend(F&& f) && {
        return taskloaf::thend(std::move(derived()), std::forward<F>(f));
    }
};

template <typename... Ts>
struct Future: public FutureBase<Future<Ts...>,Ts...> {
    using FutureBase<Future<Ts...>,Ts...>::FutureBase;
};

template <typename T>
struct Future<T>: public FutureBase<Future<T>,T> {
    using FutureBase<Future<T>,T>::FutureBase;

    using type = T;

    auto& get() & { return std::get<0>(this->get_tuple()); }
    auto&& get() && { return std::get<0>(std::move(*this).get_tuple()); }

    auto unwrap() & { return taskloaf::unwrap(*this); }
    auto unwrap() && { return taskloaf::unwrap(std::move(*this)); }
};

template <>
struct Future<>: public FutureBase<Future<>> {
    using FutureBase<Future<>>::FutureBase;

    using type = void;
};

template <>
struct Future<void>: Future<> {
    using Future<>::Future;
};

template <typename Fut, typename F>
auto apply_to(Fut&& fut, F&& fnc) {
    if (fut.is_global()) {
        auto data = fut.owner->get_ivar_tracker().get_vals(*fut.ivar);
        return apply_data_args(std::forward<F>(fnc), data);
    } else {
        return apply_args(std::forward<F>(fnc), fut.get_tuple());
    }
}

} //end namespace taskloaf

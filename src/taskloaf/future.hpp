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

    template <typename F>
    using ThenResultT = std::result_of_t<F(Ts&...)>;

    mutable TupleT val;
    mutable std::unique_ptr<IVarRef> ivar = nullptr;

    FutureBase(): ivar(std::make_unique<IVarRef>(new_id())) {}

    template <typename T,
        typename std::enable_if<
            std::is_same<typename std::decay<T>::type, TupleT>::value,void
        >::type* = nullptr
    >
    explicit FutureBase(T&& val): val(std::move(val)) {}

    FutureBase(FutureBase&& other) = default;
    FutureBase& operator=(FutureBase&& other) = default;

    FutureBase(const FutureBase& other) {
        *this = other;
    }
    FutureBase& operator=(const FutureBase& other) {
        if (cur_worker != nullptr) {
            other.ensure_at_least_global();
            ivar = std::make_unique<IVarRef>(*other.ivar);
        } else {
            //TODO: This would benefit from an intermediate state with a 
            //pointer to each element of the the tuple type, so that we don't
            //have to copy the value.
            tlassert(other.ivar == nullptr);
            val = other.val;
            ivar = nullptr;
        }
        return *this;
    }

    bool is_global() const { return ivar != nullptr; }

    bool is_local() const { return !is_global(); }

    bool can_trigger_immediately() const {
        return !is_global() || cur_worker->get_ivar_tracker().is_fulfilled_here(*ivar);
    }

    TupleT& get_tuple() & { return val; }
    TupleT&& get_tuple() && { return std::move(val); }

    void add_trigger(TriggerT trigger) const {
        ensure_at_least_global();
        cur_worker->get_ivar_tracker().add_trigger(*ivar, std::move(trigger));
    }

    void fulfill(std::vector<Data> vals) const {
        cur_worker->get_ivar_tracker().fulfill(*ivar, vals);
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
        ar(*ivar);
    }

    Derived& derived() {
        return *static_cast<Derived*>(this);
    }

    template <typename F>
    auto thend(F&& f) {
        return taskloaf::thend(derived(), std::forward<F>(f));
    }

    template <typename F>
    Future<ThenResultT<F>> then(F&& f) & {
        return taskloaf::then(derived(), std::forward<F>(f));
    }

    template <typename F>
    Future<ThenResultT<F>> then(F&& f) && {
        return taskloaf::then(std::move(derived()), std::forward<F>(f));
    }

    void wait() { 
        if (is_local()) {
            return;
        }
        bool already_thenned = false;
        //TODO: Need to mark this task as local to the current worker!
        this->then([&] (Ts&...) {
            std::cout << (intptr_t)cur_worker << std::endl;
            cur_worker->stop();
            already_thenned = true;
        });
        if (!already_thenned) {
            std::cout << (intptr_t)cur_worker << std::endl;
            cur_worker->run();
        }
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

    auto unwrap() & { return taskloaf::unwrap(*this); }
    auto unwrap() && { return taskloaf::unwrap(std::move(*this)); }

    // Always return by reference here, so that the user can decide what
    // data handling they would like: move vs. reference vs. copy.
    auto& get() { 
        if (this->is_local()) {
            return std::get<0>(this->get_tuple()); 
        }

        auto& iv_tracker = cur_worker->get_ivar_tracker();
        if (!iv_tracker.is_fulfilled_here(*this->ivar)) {
            this->wait();
        }

        return iv_tracker.get_vals(*this->ivar)[0].template get_as<T>();
    }
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
typename std::decay_t<Fut>::template ThenResultT<F> apply_to(Fut&& fut, F&& fnc) {
    if (fut.is_global()) {
        auto data = cur_worker->get_ivar_tracker().get_vals(*fut.ivar);
        return apply_data_args(std::forward<F>(fnc), data);
    } else {
        return apply_args(std::forward<F>(fnc), fut.get_tuple());
    }
}

} //end namespace taskloaf

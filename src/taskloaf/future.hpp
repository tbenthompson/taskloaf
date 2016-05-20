#pragma once

#include "builders.hpp"
#include "when_all.hpp"

#include <cereal/types/tuple.hpp>
#include <mapbox/variant.hpp>

namespace taskloaf {


template <typename Tuple, size_t... I>
auto tuple_to_data_vector(Tuple&& t, std::index_sequence<I...>) {
    return std::vector<Data>{make_data(std::get<I>(std::forward<Tuple>(t)))...};
}

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;

    Worker* owner;
    mutable mapbox::util::variant<TupleT,IVarRef> d;

    FutureBase(): owner(cur_worker), d(IVarRef(new_id())) {}
    FutureBase(Worker* owner): owner(owner), d(IVarRef(new_id())) {}

    template <
        typename T,
        typename std::enable_if<
            std::is_same<typename std::decay<T>::type, TupleT>::value,void
        >::type* = nullptr
    >
    explicit FutureBase(Worker* owner, T&& val): owner(owner), d(std::move(val)) {}

    FutureBase(FutureBase&& other) = default;
    FutureBase(const FutureBase& other) {
        *this = other;
    }

    FutureBase& operator=(FutureBase&& other) = default;
    FutureBase& operator=(const FutureBase& other) {
        owner = other.owner;
        other.make_global();
        d.template set<IVarRef>(other.d.template get<IVarRef>());
        return *this;
    }

    bool can_trigger_immediately() {
        return d.template is<TupleT>();
    }

    TupleT& get() const {
        return d.template get<TupleT>();
    }

    void make_global() const {
        if (d.template is<IVarRef>()) {
            return;
        }
        auto temp_val = std::move(d.template get<TupleT>());
        d.template set<IVarRef>(IVarRef(new_id()));
        fulfill(tuple_to_data_vector(
            std::move(temp_val), std::index_sequence_for<Ts...>{}
        ));
    }

    template <typename F>
    auto then(F&& f) {
        return taskloaf::then(*static_cast<Derived*>(this), std::forward<F>(f));
    }

    void add_trigger(TriggerT trigger) {
        make_global();
        cur_worker->add_trigger(d.template get<IVarRef>(), std::move(trigger));
    }

    void fulfill(std::vector<Data> vals) const {
        cur_worker->fulfill(d.template get<IVarRef>(), vals);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        make_global();
        ar(d.template get<IVarRef>());
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(d.template get<IVarRef>());
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
struct Future<void>: Future<> {
    using Future<>::Future;
};

} //end namespace taskloaf

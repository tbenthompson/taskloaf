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

template <typename... Ts>
struct SharedLocalData {
    bool fulfilled = false;
    std::vector<Data> data;
    std::vector<TriggerT> triggers;
};

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;
    using LocalT = SharedLocalData<Ts...>;

    Worker* owner;
    mutable mapbox::util::variant<TupleT,LocalT,IVarRef> d;

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
        other.ensure_at_least_shared();
        d = other.d;
        return *this;
    }

    bool can_trigger_immediately() const {
        return d.template is<TupleT>();
    }

    TupleT& get() const {
        return d.template get<TupleT>();
    }

    template <typename F>
    auto then(F&& f) {
        return taskloaf::then(*static_cast<Derived*>(this), std::forward<F>(f));
    }

    void add_trigger(TriggerT trigger) const {
        ensure_at_least_shared();
        if (d.template is<LocalT>()) {
            local_add_trigger(std::move(trigger));
        } else {
            cur_worker->add_trigger(d.template get<IVarRef>(), std::move(trigger));
        }
    }

    void local_add_trigger(TriggerT trigger) const {
        auto& local_d = d.template get<LocalT>();
        if (local_d.fulfilled) {
            trigger(local_d.data);
        } else {
            local_d.triggers.push_back(std::move(trigger));
        }
    }

    void fulfill(std::vector<Data> vals) const {
        if (d.template is<LocalT>()) {
            local_fulfill(std::move(vals));
        } else {
            cur_worker->fulfill(d.template get<IVarRef>(), vals);
        }
    }

    void local_fulfill(std::vector<Data> vals) const {
        auto& local_d = d.template get<LocalT>();
        local_d.data = std::move(vals);
        local_d.fulfilled = true;
        for (auto& t: local_d.triggers) {
            t(local_d.data);
        }
    }

    void ensure_at_least_shared() const {
        if (!d.template is<TupleT>()) {
            return;
        }
        auto temp_val = std::move(d.template get<TupleT>());
        d.template set<LocalT>();
        local_fulfill(tuple_to_data_vector(
            std::move(temp_val), std::index_sequence_for<Ts...>{}
        ));
    }

    void ensure_at_least_global() const {
        if (d.template is<IVarRef>()) {
            return;
        }
        ensure_at_least_shared();
        tlassert(d.template is<LocalT>());
        auto local_d = std::move(d.template get<LocalT>());
        d.template set<IVarRef>(IVarRef(new_id()));
        if (local_d.fulfilled) {
            fulfill(std::move(local_d.data));
        } else {
            for (auto& t: local_d.triggers) {
                add_trigger(std::move(t));
            }
        }
    }


    void save(cereal::BinaryOutputArchive& ar) const {
        ensure_at_least_global();
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

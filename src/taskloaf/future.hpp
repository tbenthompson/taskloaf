#pragma once

#include "builders.hpp"
#include "when_all.hpp"

#include <cereal/types/memory.hpp>

namespace taskloaf {

template <typename Func, size_t... I>
auto apply_args(Func&& func, std::vector<Data>& args, std::index_sequence<I...>) {
    return func(args[I]...);
}

template <typename T>
Data ensure_data(T&& v) {
    return make_data(std::move(v));
}

inline Data&& ensure_data(Data&& d) { return std::move(d); }
inline Data& ensure_data(Data& d) { return d; }

template <typename TupleT, size_t... I>
auto tuple_to_vector(TupleT&& t, std::index_sequence<I...>) {
    return std::vector<Data>{ensure_data(std::move(std::get<I>(t)))...};
}

// template <typename... Ts>
// struct UniqueFutureData {
//     std::tuple<UniqueData<Ts>...> vals;
// };
// 
template <typename... Ts>
struct LocalFutureData {
    bool fulfilled = false;
    std::vector<Data> vals;
    std::vector<TriggerT> triggers;
    Address owner;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;

    mutable std::shared_ptr<LocalFutureData<Ts...>> data;

    FutureBase(): data(std::make_shared<LocalFutureData<Ts...>>()) {
        data->owner = cur_worker->get_addr();
    }

    void add_trigger(TriggerT trigger) const {
        auto action = [] (std::shared_ptr<LocalFutureData<Ts...>>& data, 
            TriggerT& trigger) 
        {
            if (data->fulfilled) {
                trigger(data->vals);
            } else {
                data->triggers.push_back(std::move(trigger));
            }
        };
        if (data->owner == cur_worker->get_addr()) {
            action(data, trigger); 
        } else {
            cur_worker->get_task_collection().add_task(data->owner, TaskT(
                action, data, std::move(trigger)
            ));
        }
    }

    void fulfill_helper(std::vector<Data> vals) const {
        auto action = [] (std::shared_ptr<LocalFutureData<Ts...>>& data, 
            std::vector<Data>& vals) 
        {
            tlassert(!data->fulfilled);
            data->fulfilled = true;
            data->vals = std::move(vals);
            for (auto& t: data->triggers) {
                t(data->vals);
            }
            data->triggers.clear();
        };
        if (data->owner == cur_worker->get_addr()) {
            action(data, vals); 
        } else {
            cur_worker->get_task_collection().add_task(data->owner, TaskT(
                action, data, std::move(vals)
            ));
        }
    }

    template <size_t I>
    std::tuple_element_t<I,TupleT> get_idx() {
        static_assert(I < sizeof...(Ts), "get_idx -- index out of range");
        wait();
        tlassert(data->fulfilled);
        return data->vals[I];
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }

    FutureBase(FutureBase&& other) = default;
    FutureBase& operator=(FutureBase&& other) = default;
    FutureBase(const FutureBase& other) = default;
    FutureBase& operator=(const FutureBase& other) = default;

    template <typename T>
    void fulfill(T&& v) {
        fulfill_helper(std::vector<Data>{make_data(std::forward<T>(v))}); 
    }

    void fulfill(Data&& val) {
        fulfill_helper(std::vector<Data>{std::move(val)});
    }

    void fulfill(std::tuple<Ts...>&& val) {
        fulfill_helper(tuple_to_vector(
            std::move(val), std::index_sequence_for<Ts...>{}
        ));
    }

    template <typename F, typename... ArgTypes>
    void fulfill_with(F&& f, std::vector<Data>& args) {
        fulfill(apply_args(
            f, args, std::index_sequence_for<ArgTypes...>{}
        ));
    }

    template <typename F>
    void fulfill_with(F&& f) {
        fulfill(f());
    }

    template <typename F>
    auto then(F&& f) {
        return then(Loc::anywhere, std::forward<F>(f));
    }

    template <typename F>
    auto then(Loc loc, F&& f) {
        return then(static_cast<int>(loc), std::forward<F>(f));
    }

    template <typename F>
    auto then(int loc, F&& f) {
        return taskloaf::then(loc, *static_cast<Derived*>(this), std::forward<F>(f));
    }

    void wait() { 
        cur_worker->set_stopped(false);
        bool already_thenned = false;
        this->then(Loc::here, [&] (Ts&...) {
            cur_worker->set_stopped(true);
            already_thenned = true;
        });
        if (!already_thenned) {
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

    auto unwrap() {
        return taskloaf::unwrap(*this); 
    }

    // I'd like to return by reference here, so that the user can decide what
    // data handling they would like: move vs. reference vs. copy.
    // TODO: Need caching of data for returning a reference.
    T get() { 
        return this->template get_idx<0>();
    }
};

template <>
struct Future<>: public FutureBase<Future<>> {
    using FutureBase<Future<>>::FutureBase;

    using type = void;

    void fulfill() {
        fulfill_helper(std::vector<Data>{});
    }

    template <typename F, typename... ArgTypes>
    void fulfill_with(F&& f, std::vector<Data>& args) {
        apply_args(f, args, std::index_sequence_for<ArgTypes...>{});
        fulfill();
    }

    template <typename F>
    void fulfill_with(F&& f) {
        f();
        fulfill();
    }
};

template <>
struct Future<void>: Future<> {
    using Future<>::Future;
};

inline Future<> empty() {
    Future<> out_future;
    out_future.fulfill();
    return out_future;
}

} //end namespace taskloaf

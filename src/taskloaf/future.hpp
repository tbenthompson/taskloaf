#pragma once

#include "builders.hpp"
#include "when_all.hpp"
#include "global_ref.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename Derived, typename... Ts>
struct FutureBase {
    using TupleT = std::tuple<Ts...>;

    mutable GlobalRef gref;

    FutureBase(): gref(new_id()) {}
    FutureBase(FutureBase&& other) = default;
    FutureBase& operator=(FutureBase&& other) = default;
    FutureBase(const FutureBase& other) = default;
    FutureBase& operator=(const FutureBase& other) = default;

    void add_trigger(TriggerT trigger) const {
        cur_worker->get_ref_tracker().add_trigger(gref, std::move(trigger));
    }

    void fulfill(std::vector<Data> vals) const {
        cur_worker->get_ref_tracker().fulfill(gref, vals);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(gref);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(gref);
    }

    template <typename F>
    auto then(F&& f) {
        return then(Loc::anywhere, std::forward<F>(f));
    }

    template <typename F>
    auto then(Loc loc, F&& f) {
        return taskloaf::then(
            static_cast<int>(loc), *static_cast<Derived*>(this),
            std::forward<F>(f)
        );
    }

    template <typename F>
    auto then(int loc, F&& f) {
        return taskloaf::then(loc, *static_cast<Derived*>(this), std::forward<F>(f));
    }

    void wait() { 
        bool already_thenned = false;
        this->then(Loc::here, [&] (Ts&...) {
            cur_worker->stop();
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

    // TODO: Always return by reference here, so that the user can decide what
    // data handling they would like: move vs. reference vs. copy.
    // Need to solve the data caching issue first.
    T get() { 
        auto& iv_tracker = cur_worker->get_ref_tracker();

        T out;
        if (iv_tracker.is_fulfilled_here(this->gref)) {
            out = iv_tracker.get_vals(this->gref)[0].template get_as<T>();
        } else {
            // TODO: This duplication wouldn't be necessary with proper data
            // caching and could be replaced by a wait and a tracker grab.
            bool already_thenned = false;
            this->then(Loc::here, [&] (T& v) {
                out = v;
                cur_worker->stop();
                already_thenned = true;
            });
            if (!already_thenned) {
                cur_worker->run();
            }
        }

        return out;
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

inline Future<> empty() {
    Future<> out_future;
    out_future.fulfill({});
    return out_future;
}

} //end namespace taskloaf

#pragma once

#include "fnc.hpp"
#include "data.hpp"
#include "execute.hpp"

#include <cereal/types/vector.hpp>

namespace taskloaf {

template <typename F>
struct ClosureBase {};
template <typename ClosureType>
struct SerializableClosure {};

template <typename Return, typename... Args>
using SerializableClosurePtr = std::unique_ptr<SerializableClosure<Return(Args...)>>;

template <typename Return, typename... Args>
struct ClosureBase<Return(Args...)> {
    virtual Return operator()(Args... args) = 0;
    virtual bool is_serializable() = 0;
    virtual SerializableClosurePtr<Return,Args...> make_serializable() = 0;
};

template <typename Return, typename... Args>
struct SerializableClosure<Return(Args...)>: public ClosureBase<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args&...)> f;
    std::vector<Data> vs;

    SerializableClosure() = default;
    SerializableClosure(Function<Return(std::vector<Data>&,Args&...)> f,
        std::vector<Data> vs):
        f(f),
        vs(vs)
    {}

    Return operator()(Args... args) {
        return f(vs, args...);
    }

    bool is_serializable() override {
        return true;
    }

    SerializableClosurePtr<Return,Args...> make_serializable() override {
        return std::make_unique<SerializableClosure<Return(Args...)>>(*this);
    }
};

template <typename ClosureType, typename F, typename... Ts>
struct TypedClosure {};
template <typename Return, typename... Args, typename F, typename... Ts>
struct TypedClosure<Return(Args...),F,Ts...>: public ClosureBase<Return(Args...)> {
    constexpr static std::index_sequence_for<Ts...> idxs{};

    F f;
    // TODO: Maybe use std::tuple<TypedData<Ts>...> here?
    std::tuple<Ts...> vs;

    TypedClosure(std::tuple<Ts...>&& vs, F&& f):
        f(std::move(f)),
        vs(std::move(vs))
    {}

    Return operator()(Args... args) override {
        return call(idxs, args...);
    }

    template <size_t... I>
    Return call(std::index_sequence<I...>, Args... args) {
        return f(std::get<I>(vs)..., args...);
    }

    bool is_serializable() override {
        return false;
    }

    SerializableClosurePtr<Return,Args...> make_serializable() override {
        return make_serializable_helper(idxs);
    }

    template <size_t... I>
    auto make_serializable_helper(std::index_sequence<I...>) {
        auto f_serializable = make_function(f);
        return std::make_unique<SerializableClosure<Return(Args...)>>(
            [] (std::vector<Data>& cargs, Args... args) {
                auto& f = cargs[0].get_as<F>();
                return f(
                    cargs[I+1].get_as<
                        std::tuple_element_t<I,std::tuple<Ts...>>
                    >()...,
                    args...
                );
            }, 
            std::vector<Data>{make_data(f_serializable), make_data(std::get<I>(vs))...}
        );
    }
};

template <typename F>
struct Closure {};
template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    mutable std::unique_ptr<ClosureBase<Return(Args...)>> ptr;


    template <typename F, typename... Ts>
    Closure(F&& f, Ts&&... vs):
        ptr(std::make_unique<TypedClosure<Return(Args...),F,std::decay_t<Ts>...>>(
            std::make_tuple(std::forward<Ts>(vs)...), std::forward<F>(f)
        ))
    {}
    Closure(Function<Return(std::vector<Data>&,Args&...)> f,
        std::vector<Data> vs):
        ptr(std::make_unique<SerializableClosure<Return(Args...)>>(f, std::move(vs)))
    {}
    Closure() = default;

    Return operator()(Args... args) {
        return ptr->operator()(args...);
    }

    auto* get_serializable() const {
        if (!ptr->is_serializable()) {
            ptr = ptr->make_serializable();
        }
        return reinterpret_cast<SerializableClosure<Return(Args...)>*>(ptr.get());
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        auto* s_ptr = get_serializable();
        ar(s_ptr->f);
        ar(s_ptr->vs);
    }

    void load(cereal::BinaryInputArchive& ar) {
        auto s_ptr = std::make_unique<SerializableClosure<Return(Args...)>>();
        ar(s_ptr->f);
        ar(s_ptr->vs);
        ptr = std::move(s_ptr);
    }
};

typedef Closure<Data(std::vector<Data>&)> PureTaskT;
typedef Closure<void(std::vector<Data>&)> TriggerT;
typedef Closure<void()> TaskT;

} //end namespace taskloaf

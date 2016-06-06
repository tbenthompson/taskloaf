#pragma once

#include "fnc.hpp"
#include "data.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/tuple.hpp>
#include <cstring>
#include <algorithm>

namespace taskloaf {

template <typename F, typename = void>
struct BytesIfNotSerializable {
    std::array<uint8_t,sizeof(F)> data;

    BytesIfNotSerializable(F f) {
        std::copy_n(reinterpret_cast<uint8_t*>(&f), sizeof(F), data.begin()); 
    }
    BytesIfNotSerializable() = default;

    void save(cereal::BinaryOutputArchive& ar) const {
        ar.saveBinary(data.begin(), sizeof(F));
    }
    void load(cereal::BinaryInputArchive& ar) {
        ar.loadBinary(data.begin(), sizeof(F));
    }

    F& get() { return *reinterpret_cast<F*>(data.begin()); }
};

template <typename F>
struct BytesIfNotSerializable<
    F, std::enable_if_t<is_serializable<std::decay_t<F>>::value>
>  
{
    F f;

    BytesIfNotSerializable(F f): f(f) {}
    BytesIfNotSerializable() = default;

    void save(cereal::BinaryOutputArchive& ar) const { ar(f); }
    void load(cereal::BinaryInputArchive& ar) { ar(f); }

    F& get() { return f; }
};

template <typename F>
struct ClosureBase {};

template <typename Return, typename... Args>
struct ClosureBase<Return(Args...)> {
    virtual Return operator()(Args... args) = 0;
    virtual void save(cereal::BinaryOutputArchive& ar) const = 0;
    virtual std::pair<int,int> get_deserializer_id() const = 0;
};

template <typename ClosureType, typename F, typename... Ts>
struct TypedClosure {};
template <typename Return, typename... Args, typename F, typename... Ts>
struct TypedClosure<Return(Args...),F,Ts...>: public ClosureBase<Return(Args...)> {
    constexpr static std::index_sequence_for<Ts...> idxs{};
    using TupleT = std::tuple<EnsureDataT<Ts>...>;

    BytesIfNotSerializable<F> f;
    std::tuple<EnsureDataT<Ts>...> vs;

    TypedClosure(F f, TupleT vs): f(std::move(f)), vs(std::move(vs)) {}
    TypedClosure() = default;

    Return operator()(Args... args) override {
        return call(idxs, args...);
    }

    template <size_t... I>
    Return call(std::index_sequence<I...>, Args... args) {
        return f.get()(std::get<I>(vs)..., args...);
    }

    std::pair<int,int> get_deserializer_id() const override {
        auto f = [] (cereal::BinaryInputArchive& ar) {
            auto out = std::make_unique<TypedClosure<Return(Args...),F,Ts...>>();
            ar(*out);
            return out;
        };

        return RegisterFnc<
            decltype(f),
            std::unique_ptr<ClosureBase<Return(Args...)>>,
            cereal::BinaryInputArchive&
        >::add_to_registry();
    }

    void save(cereal::BinaryOutputArchive& ar) const override {
        ar(f);
        ar(vs);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(f);
        ar(vs);
    }
};

template <typename F>
struct Closure {};
template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    using DerivedPtr = std::unique_ptr<ClosureBase<Return(Args...)>>;
    using DeserializerT = DerivedPtr(*)(const char*,cereal::BinaryInputArchive&);
    DerivedPtr ptr;

    template <typename F, typename... Ts>
    Closure(F&& f, Ts&&... vs):
        ptr(std::make_unique<
                TypedClosure<Return(Args...),std::decay_t<F>,std::decay_t<Ts>...>
            >(
                std::forward<F>(f),
                std::make_tuple(ensure_data(std::forward<Ts>(vs))...)
            )
        )
    {}

    Closure() = default;

    bool operator==(std::nullptr_t) const {
        return ptr == nullptr;
    }

    bool operator!=(std::nullptr_t) const {
        return !(ptr == nullptr);
    }

    Return operator()(Args... args) {
        return ptr->operator()(ensure_data(args)...);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(ptr->get_deserializer_id());
        ptr->save(ar);
    }

    void load(cereal::BinaryInputArchive& ar) {
        std::pair<int,int> caller_id;
        ar(caller_id);
        auto caller = reinterpret_cast<DeserializerT>(
            get_caller_registry().get_function(caller_id)
        );
        char* empty = nullptr;
        ptr = caller(empty, ar);
    }
};

template <typename F>
auto make_closure(F&& f) {
    return Closure<GetSignature<F>>(std::forward<F>(f));
}

} //end namespace taskloaf

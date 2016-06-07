#pragma once

#include "fnc_registry.hpp"
#include "data.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/array.hpp>

namespace taskloaf {

template <typename F, size_t... I, typename... Args>
auto closure_apply(F& f, std::vector<Data>& d, std::index_sequence<I...>,
    Args&&... args) 
{
    return f(d[I].convertible()..., std::forward<Args>(args)...);
}

template <typename F>
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    using CallerT = Return (*) (Closure<Return(Args...)>&,Args...);
    using SerializerT = void (*)(cereal::BinaryOutputArchive&);
    using DeserializerT = void (*)(
        const char*,Closure<Return(Args...)>&,cereal::BinaryInputArchive&
    );

    Data f;
    std::vector<Data> d;
    CallerT caller;
    SerializerT serializer;

    template <typename F, typename... Ts>
    static void closure_serializer(cereal::BinaryOutputArchive& ar) 
    {
        auto f = [] (Closure<Return(Args...)>& c, cereal::BinaryInputArchive& ar) {
            ar(c.f);
            ar(c.d);
            c.caller = &closure_caller<F,Ts...>;
            c.serializer = &closure_serializer<F,Ts...>;
        };
        auto deserializer_id = RegisterFnc<
            decltype(f),void,Closure<Return(Args...)>&,cereal::BinaryInputArchive&
        >::add_to_registry();
        ar(deserializer_id);
    }

    template <typename F, typename... Ts>
    static Return closure_caller(Closure<Return(Args...)>& c, Args... args) {
        return closure_apply(
            c.f.template unchecked_get_as<F>(), c.d, std::index_sequence_for<Ts...>{},
            std::forward<Args>(args)...
        );
    }

    template <typename F, typename... Ts>
    Closure(F f, Ts&&... vs):
        f(make_data(*reinterpret_cast<std::array<uint8_t,sizeof(F)>*>(&f))),
        d{ensure_data(std::forward<Ts>(vs))...}
    {
        static_assert(
            std::is_same<std::result_of_t<F(Ts&...,Args...)>,Return>::value,
            "Function in Closure has the wrong return type."
        );
        static_assert(
            !is_serializable<std::decay_t<F>>::value,
            "The function type for Closure cannot be serializable"
        );
        caller = &closure_caller<F,Ts...>;
        serializer = &closure_serializer<F,Ts...>;
    }

    Closure() = default;

    bool operator==(std::nullptr_t) const { return f == nullptr; }

    Return operator()(Args... args) {
        return caller(*this, std::forward<Args>(args)...);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        serializer(ar);
        ar(f);
        ar(d);
    }

    void load(cereal::BinaryInputArchive& ar) {
        std::pair<int,int> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<DeserializerT>(
            get_caller_registry().get_function(deserializer_id)
        );
        const char* x = "";
        deserializer(x, *this, ar);
    }
};

} //end namespace taskloaf

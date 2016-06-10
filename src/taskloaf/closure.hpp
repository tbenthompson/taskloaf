#pragma once

#include "fnc_registry.hpp"
#include "sized_any.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/tuple.hpp>

namespace taskloaf {

template <typename RetT, typename FncT, typename EncT, typename ArgT>
struct closure {
    using caller_type = RetT(*)(closure&,ArgT&);
    using serializer_type = void(*)(cereal::BinaryOutputArchive&);
    using deserializer_type = void(*)(closure&,cereal::BinaryInputArchive&);

    FncT f;
    EncT d;
    caller_type caller;
    serializer_type serializer;

    closure() = default;

    template <typename F>
    static void serializer_fnc(cereal::BinaryOutputArchive& ar) 
    {
        auto f = [] (closure& c, cereal::BinaryInputArchive& ar) {
            ar(c.f);
            ar(c.d);
            c.caller = &caller_fnc<F>;
            c.serializer = &serializer_fnc<F>;
        };
        auto deserializer_id = register_fnc<
            decltype(f),void,closure&,cereal::BinaryInputArchive&
        >::add_to_registry();
        ar(deserializer_id);
    }

    template <typename F>
    static auto caller_fnc(closure& c, ArgT& arg) {
        return ensure_any(c.f.template unchecked_get<F>()(c.d, arg));
    }

    bool operator==(std::nullptr_t) const {
        return caller == nullptr; 
    }
    bool operator!=(std::nullptr_t) const {
        return !(*this == nullptr);
    }

    RetT operator()(ArgT& arg) {
        return caller(*this, arg);
    }

    RetT operator()(ArgT&& arg) {
        return caller(*this, arg);
    }

    RetT operator()() {
        auto d = ensure_any();
        return caller(*this, d);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        serializer(ar);
        ar(f);
        ar(d);
    }

    void load(cereal::BinaryInputArchive& ar) {
        std::pair<size_t,size_t> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<deserializer_type>(
            get_fnc_registry().get_function(deserializer_id)
        );
        deserializer(*this, ar);
    }
};

template<typename T>
struct get_signature: 
    public get_signature<decltype(&std::decay_t<T>::operator())> {};

template <typename Return, typename Arg1, typename Arg2>
struct get_signature<Return(Arg1,Arg2)> {
    using return_type = Return;
    using arg1_type = Arg1;
    using arg2_type = Arg2;
};

template <typename Return, typename Class, typename Arg1, typename Arg2>
struct get_signature<Return(Class::*)(Arg1,Arg2)>: 
    public get_signature<Return(Arg1,Arg2)> {};

template <typename Return, typename Class, typename Arg1, typename Arg2>
struct get_signature<Return(Class::*)(Arg1,Arg2) const>: 
    public get_signature<Return(Arg1,Arg2)> {};

template <typename Return, typename Arg1, typename Arg2>
struct get_signature<Return(&)(Arg1,Arg2)>: 
    public get_signature<Return(Arg1,Arg2)> {};

template <typename Return, typename Arg1, typename Arg2>
struct get_signature<Return(*)(Arg1,Arg2)>: 
    public get_signature<Return(Arg1,Arg2)> {};

template <typename F, typename T>
auto make_closure(F fnc, T val) {
    auto f = ensure_any(std::move(fnc));
    auto d = ensure_any(std::move(val));

    using signature = get_signature<F>;

    using return_type = decltype(
        ensure_any(std::declval<typename signature::return_type>())
    );
    using arg_type = typename signature::arg2_type;
    using closure_type = closure<return_type,decltype(f),decltype(d),arg_type>;

    return closure_type{
        f, d, 
        closure_type::template caller_fnc<F>,
        closure_type::template serializer_fnc<F>
    };
}

template <typename F>
auto make_closure(F fnc) {
    return make_closure(std::move(fnc), ensure_any());
}

} //end namespace taskloaf

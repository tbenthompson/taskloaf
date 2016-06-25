#pragma once

#include "debug.hpp"
#include "data.hpp"
#include "fnc_registry.hpp"

#include <pybind11/pybind11.h>

namespace taskloaf {

struct closure {
    using caller_type = data(*)(closure&,data&);
    using serializer_type = void(*)(cereal::BinaryOutputArchive&);
    using deserializer_type = void(*)(closure&,cereal::BinaryInputArchive&);

    data f;
    data d;
    caller_type caller;
    serializer_type serializer;

    closure() = default;

    template <typename F, typename T>
    closure(F fnc, T&& val) {
        static_assert(!std::is_function<std::remove_pointer_t<F>>::value,
            "Closure function must be a lambda or serializable functor object.");
        static_assert(std::is_trivially_copyable<std::decay_t<F>>::value,
            "Closure function must be trivially copyable.");

        f = data(std::move(fnc));
        d = data(std::forward<T>(val));

        caller = closure::template caller_fnc<std::decay_t<F>>;
        serializer = closure::template serializer_fnc<std::decay_t<F>>;
    }

    template <typename F, 
        std::enable_if_t<!std::is_same<std::decay_t<F>,closure>::value>* = nullptr>
    closure(F fnc):
        closure(std::move(fnc), ignore{}) 
    {}

    template <typename F>
    static void serializer_fnc(cereal::BinaryOutputArchive& ar) {
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
    static data caller_fnc(closure& c, data& arg) {
        return data(c.f.template get<F>()(c.d, arg));
    }

    bool empty() {
        return f.empty();
    }

    operator bool() {
        return !empty();
    }

    data operator()(data& arg) {
        return caller(*this, arg);
    }

    data operator()(data&& arg) {
        return caller(*this, arg);
    }

    data operator()() {
        static auto d = data(ignore{});
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

} //end namespace taskloaf

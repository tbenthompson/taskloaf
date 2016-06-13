#pragma once

#include "debug.hpp"
#include "data.hpp"
#include "fnc_registry.hpp"

namespace taskloaf {

struct closure {
    using caller_type = data_ptr(*)(closure&,data_ptr&);
    using serializer_type = void(*)(cereal::BinaryOutputArchive&);
    using deserializer_type = void(*)(closure&,cereal::BinaryInputArchive&);

    data_ptr f;
    data_ptr d;
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
    static data_ptr caller_fnc(closure& c, data_ptr& arg) {
        return make_data(c.f.template get<F>()(c.d, arg));
    }

    bool empty() {
        return f.empty();
    }

    data_ptr operator()(data_ptr& arg) {
        return caller(*this, arg);
    }

    data_ptr operator()(data_ptr&& arg) {
        return caller(*this, arg);
    }

    data_ptr operator()() {
        static auto d = make_data(ignore{});
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

template <typename F, typename T>
auto make_closure(F fnc, T val) {
    static_assert(std::is_trivially_copyable<F>::value,
        "Function type must be trivially copyable");
    auto f = make_data(std::move(fnc));
    auto d = make_data(std::move(val));

    return closure{
        std::move(f), std::move(d), 
        closure::template caller_fnc<F>,
        closure::template serializer_fnc<F>
    };
}

template <typename F>
auto make_closure(F fnc) {
    return make_closure(std::move(fnc), ignore{});
}

} //end namespace taskloaf

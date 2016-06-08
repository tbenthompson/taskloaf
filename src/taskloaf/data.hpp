#pragma once

#include "tlassert.hpp"
#include "fnc_registry.hpp"

#ifdef TLDEBUG
#include <typeinfo>
#endif

#include <cereal/archives/binary.hpp>

namespace taskloaf {

template <typename F>
struct is_serializable: std::integral_constant<bool,
    cereal::traits::is_output_serializable<F,cereal::BinaryOutputArchive>::value &&
    cereal::traits::is_input_serializable<F,cereal::BinaryInputArchive>::value &&
    !std::is_pointer<F>::value> {};

struct Data;
struct ConvertibleData {
    Data& d;
    
    template <typename T>
    operator T&();

    template <typename T>
    operator const T&() const;
};

// Situations:
// 1. Large type, shared reference --> pool
// 2. Small type, shared reference --> pool
// 3. Large type, unique reference --> pool
// 4. Small type, unique reference --> inside
struct Data {
    using SerializerT = void (*)(const Data&,cereal::BinaryOutputArchive&);
    using DeserializerT = void (*)(Data&,cereal::BinaryInputArchive&);
    std::shared_ptr<void> ptr;
    SerializerT serializer;
#ifdef TLDEBUG
    // type_info refers to a static lifetime object managed internally by the
    // runtime, but it's copy constructor is deleted. So, in order to track the
    // object's type across copies, I put the type in a shared_ptr. shared_ptr
    // cannot contain a reference, so I use reference_wrapper.
    std::shared_ptr<std::reference_wrapper<const std::type_info>> type;
#endif

    template <typename T, typename... Ts>
    void initialize(Ts&&... args) {
#ifdef TLDEBUG
        type = std::make_shared<std::reference_wrapper<const std::type_info>>(typeid(T));
#endif
        ptr.reset(new T(std::forward<Ts>(args)...), [] (void* data_ptr) {
            delete reinterpret_cast<T*>(data_ptr);
        });
        serializer = &save_fnc<T>;
    }

    template <typename T>
    static void save_fnc(const Data& d, cereal::BinaryOutputArchive& ar) {
        tlassert(d.ptr != nullptr);

        // Create and register the deserialization function that will be called
        // on the other end of the wire. This is done here as opposed to in 
        // initialize so that serialization is zero-overhead unless it is used.
        auto deserializer = [] (Data& d, cereal::BinaryInputArchive& ar) {
#ifdef TLDEBUG
            d.type = std::make_shared<
                std::reference_wrapper<const std::type_info>
            >(typeid(T));
#endif
            d.initialize<T>();
            ar(d.get_as<T>());
        };
        auto deserializer_id = RegisterFnc<
            decltype(deserializer),void,
            Data&,cereal::BinaryInputArchive&
        >::add_to_registry();

        ar(deserializer_id);
        ar(d.get_as<T>());
    };
    
    template <typename T>
    T& unchecked_get_as() {
        return *reinterpret_cast<T*>(ptr.get()); 
    }

    template <typename T>
    T& get_as() {
        // In debug builds, do a runtime check to make sure the data type is
        // correct.
        tlassert(typeid(T) == *type); 
        tlassert(ptr != nullptr);
        return unchecked_get_as<T>();
    }

    template <typename T>
    const T& get_as() const {
        return const_cast<Data*>(this)->get_as<T>();
    }

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
    bool operator==(std::nullptr_t) const;
    ConvertibleData convertible();
};

template <typename T>
ConvertibleData::operator T&() {
    return d.get_as<T>();
}

template <typename T>
ConvertibleData::operator const T&() const {
    return d.get_as<T>();
}

template <typename T>
auto make_data(T&& v) {
    static_assert(
        is_serializable<std::decay_t<T>>::value,
        "Data requires the contained type be serializable"
    );
    Data d;
    d.initialize<std::decay_t<T>>(std::forward<T>(v));
    return d;
}

template <typename T,
    std::enable_if_t<!std::is_same<std::decay_t<T>,Data>::value>* = nullptr>
auto ensure_data(T&& v) {
    return make_data(std::forward<T>(v));
}

template <typename T,
    std::enable_if_t<std::is_same<std::decay_t<T>,Data>::value>* = nullptr>
auto ensure_data(T&& v) {
    return std::forward<T>(v);
}

} //end namespace taskloaf

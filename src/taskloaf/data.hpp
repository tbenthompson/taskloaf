#pragma once

#include "tlassert.hpp"
#include "fnc_registry.hpp"

#ifdef TLDEBUG
#include <typeinfo>
#endif

#include <cereal/archives/binary.hpp>

namespace taskloaf {

template <typename F>
struct IsSerializable: std::integral_constant<bool,
    cereal::traits::is_output_serializable<F,cereal::BinaryOutputArchive>::value &&
    cereal::traits::is_input_serializable<F,cereal::BinaryInputArchive>::value &&
    !std::is_pointer<F>::value> {};

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
    // runtime, so there's no need to worry about ownership of a pointer.
    const std::type_info* type;
#endif

    Data() = default;

    template <typename T, typename... Ts>
    void initialize(Ts&&... args) {
        using DecayT = std::decay_t<T>;
#ifdef TLDEBUG
        type = &typeid(T);
#endif
        ptr.reset(new DecayT(std::forward<Ts>(args)...), [] (void* data_ptr) {
            delete reinterpret_cast<DecayT*>(data_ptr);
        });
        serializer = &save_fnc<DecayT>;
    }

    template <typename T>
    static void save_fnc(const Data& d, cereal::BinaryOutputArchive& ar) {
        tlassert(d.ptr != nullptr);

        // Create and register the deserialization function that will be called
        // on the other end of the wire. This is done here as opposed to in 
        // initialize so that serialization is zero-overhead unless it is used.
        auto deserializer = [] (Data& d, cereal::BinaryInputArchive& ar) {
#ifdef TLDEBUG
            d.type = &typeid(T);
#endif
            d.initialize<T>();
            ar(d.get_as<T>());
        };
        auto deserializer_id = register_fnc<
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
#ifdef TLDEBUG
        if (typeid(T) != *type) {
            std::cout << type->name() << "     " << typeid(T).name() << std::endl;
            tlassert(typeid(T) == *type); 
        }
#endif
        tlassert(ptr != nullptr);
        return unchecked_get_as<T>();
    }

    template <typename T>
    const T& get_as() const {
        return const_cast<Data*>(this)->get_as<T>();
    }
    
    template <typename T>
    operator T&() {
        return get_as<T>();
    }

    template <typename T>
    operator const T&() const {
        return get_as<T>();
    }

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
};

template <typename T>
struct SerializeAsBytes: std::integral_constant<bool,
    std::is_trivially_copyable<std::decay_t<T>>::value &&
    !IsSerializable<std::decay_t<T>>::value> {};

template <typename T, std::enable_if_t<!SerializeAsBytes<T>::value>* = nullptr>
Data ensure_data(T&& v) {
    using DecayT = std::decay_t<T>;
    Data d;
    d.initialize<DecayT>(std::forward<T>(v));
    return d;
}

template <typename T, std::enable_if_t<SerializeAsBytes<T>::value>* = nullptr>
Data ensure_data(T&& v) {
    using DecayT = std::decay_t<T>;
    using ArrayT = std::array<uint8_t,sizeof(DecayT)>;
    Data d;
    d.initialize<ArrayT>(*reinterpret_cast<ArrayT*>(&v));
    return d;
}

Data ensure_data(Data& d);
Data ensure_data(Data&& d);

} //end namespace taskloaf

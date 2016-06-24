#pragma once

#include "debug.hpp"
#include "fnc_registry.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

struct ignore {};
using _ = ignore;

struct data {
    using deserializer_type = data(*)(cereal::BinaryInputArchive&);
    using serializer_type = void(*)(const data&,cereal::BinaryOutputArchive&);

    std::shared_ptr<void> ptr;
    serializer_type serializer = nullptr;
    TL_IF_DEBUG(const std::type_info* type;,)

    data() = default;

    template <typename T,
        std::enable_if_t<!std::is_same<std::decay_t<T>,data>::value>* = nullptr>
    data(T&& v):
        ptr(new std::decay_t<T>(std::forward<T>(v)), [] (void* data_ptr) {
            delete reinterpret_cast<std::decay_t<T>*>(data_ptr);
        }),
        serializer(serialize<std::decay_t<T>>)
#define COMMA ,
        TL_IF_DEBUG(COMMA type(&typeid(std::decay_t<T>)),)
#undef COMMA
    {}

    bool empty() const {
        return serializer == nullptr;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        serializer(*this, ar);
    }

    void load(cereal::BinaryInputArchive& ar) { 
        std::pair<size_t,size_t> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<deserializer_type>(
            get_fnc_registry().get_function(deserializer_id)
        );
        *this = deserializer(ar);
    }

    template <typename T>
    T& unchecked_get() {
        return *reinterpret_cast<T*>(ptr.get());
    }

    template <typename T>
    T& get() {
        TL_IF_DEBUG(
        bool type_check = (*type) == typeid(T) || typeid(ignore) == typeid(T);
        if (!(type_check)) {
            std::cout << "Runtime type check failed: " << std::endl;
            std::cout << type->name() << " " << typeid(T).name() << std::endl;
        }
        ,)
        TLASSERT(type_check);
        return unchecked_get<T>();
    }

    template <typename T>
    const T& get() const { return const_cast<data*>(this)->template get<T>(); }

    template <typename T>
    operator T&() { return get<T>(); }

    template <typename T, 
        std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
    static void data_save(const T& v, cereal::BinaryOutputArchive& ar) {
        ar(v);
    }

    template <typename T, 
        std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
    static void data_save(const T& v, cereal::BinaryOutputArchive& ar) {
        ar.saveBinary(&v, sizeof(T));
    }

    template <typename T, 
        std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
    static data data_load(cereal::BinaryInputArchive& ar) {
        T v;
        ar(v);
        return data(std::move(v));
    }

    template <typename T, 
        std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
    static data data_load(cereal::BinaryInputArchive& ar) {
        std::array<uint8_t,sizeof(T)> v;
        ar.loadBinary(&v, sizeof(T));
        return data(std::move(*reinterpret_cast<T*>(&v)));
    }

    template <typename T>
    static void serialize(const data& d, cereal::BinaryOutputArchive& ar) {
        auto deserialize_fnc = [] (cereal::BinaryInputArchive& ar) {
            return data_load<T>(ar);
        };
        auto deserializer_id = register_fnc<
            decltype(deserialize_fnc),data,cereal::BinaryInputArchive&
        >::add_to_registry();
        ar(deserializer_id);
        
        data_save<T>(d.template get<T>(), ar);
    }
};

} //end namespace taskloaf

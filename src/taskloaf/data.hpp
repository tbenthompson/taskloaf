#pragma once

#include "fnc.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>

#include <memory>

namespace taskloaf {

/* This is a wrapper that hides an arbitrary object inside a shared_ptr and 
 * allows serialization and deserialization of that object.
 *
 * Requirements of the types that can be placed in Data:
 * 1) Default constructor, assignable
 * 2) cereal's serialize function is defined
 * 3) 
 */
struct Data {
    std::shared_ptr<void> ptr;
    std::function<void(const Data&,cereal::BinaryOutputArchive&)> serializer;
    Function<void(Data&,cereal::BinaryInputArchive&)> deserializer;

    template <typename T, typename... Ts>
    void initialize(Ts&&... args) {
        ptr.reset(new T(std::forward<Ts>(args)...), [] (void* data_ptr) {
            delete reinterpret_cast<T*>(data_ptr);
        });
        serializer = [] (const Data& d, cereal::BinaryOutputArchive& ar) {
            tlassert(d.ptr != nullptr);
            ar(d.get_as<T>());
        };
        deserializer = [] (Data& d, cereal::BinaryInputArchive& ar) {
            d.initialize<T>();
            ar(d.get_as<T>());
        };
    }

    template <typename Archive>
    void save(Archive& ar) const {
        tlassert(ptr != nullptr);
        ar(deserializer);
        serializer(*this, ar);
    }

    template <typename Archive>
    void load(Archive& ar) {
        ar(deserializer);
        deserializer(*this, ar);
    }

    template <typename T>
    T& get_as() {
        tlassert(ptr != nullptr);
        return *reinterpret_cast<T*>(ptr.get()); 
    }

    template <typename T>
    const T& get_as() const {
        return const_cast<Data*>(this)->get_as<T>();
    }
};

template <typename T, typename... Ts>
Data make_data(Ts&&... args) {
    Data d;
    d.initialize<T>(std::forward<Ts>(args)...);
    return d;
}

template <typename T>
Data make_data(T&& a) {
    Data d;
    d.initialize<typename std::decay<T>::type>(std::forward<T>(a));
    return d;
}

inline Data empty_data() {
    return Data();
}

} //end namespace taskloaf

#pragma once

#include "fnc.hpp"

namespace taskloaf {

template <typename T>
struct UniqueData {
    T val;
};

template <typename T>
struct TypedData {
    //variant of UniqueData<T> and Data
};

struct Data;
template <typename T>
void serializer(const Data& d, cereal::BinaryOutputArchive& ar);

// Maybe worthwhile to think about ownership more, and create something like
// a unique data and a shared data?
struct Data {
    std::shared_ptr<void> ptr;
    void (*my_serializer)(const Data&,cereal::BinaryOutputArchive&);

    template <typename T, typename... Ts>
    void initialize(Ts&&... args) {
        ptr.reset(new T(std::forward<Ts>(args)...), [] (void* data_ptr) {
            delete reinterpret_cast<T*>(data_ptr);
        });
        my_serializer = &serializer<T>;
    }

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    template <typename T>
    T& get_as() {
        tlassert(ptr != nullptr);
        return *reinterpret_cast<T*>(ptr.get()); 
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
};

template <typename T>
void deserializer(Data& d, cereal::BinaryInputArchive& ar) {
    d.initialize<T>();
    ar(d.get_as<T>());
};

template <typename T>
void serializer(const Data& d, cereal::BinaryOutputArchive& ar) {
    tlassert(d.ptr != nullptr);
    ar(make_function(deserializer<T>));
    ar(d.get_as<T>());
};

template <typename T>
Data make_data(T&& a) {
    static_assert(is_serializable<std::decay_t<T>>::value,
        "Types encapsulated in Data must be serializable. Please Provide serialization functions for this type."
    );

    Data d;
    d.initialize<std::decay_t<T>>(std::forward<T>(a));
    return d;
}

Data empty_data();

} //end namespace taskloaf

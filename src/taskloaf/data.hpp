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
    std::function<std::string(const Data&)> serializer;
    Function<void(Data&,const std::string&)> deserializer;

    Data():
        ptr(nullptr),
        serializer(nullptr)
    {}

    template <
        typename T, 
        typename std::enable_if<
            !std::is_same<typename std::decay<T>::type,Data>::value
        >::type* = nullptr
    >
    explicit Data(T&& value) {
        typedef typename std::decay<T>::type DecayedT;
        initialize<DecayedT>();
        *reinterpret_cast<DecayedT*>(ptr.get()) = std::forward<T>(value);
    }

    template <typename T>
    void initialize() {
        ptr.reset(new T(), [] (void* data_ptr) {
            delete reinterpret_cast<T*>(data_ptr);
        });
        serializer = [] (const Data& d) {
            std::stringstream serialized_data;
            cereal::BinaryOutputArchive oarchive(serialized_data);
            oarchive(*reinterpret_cast<T*>(d.ptr.get()));
            return serialized_data.str();
        };
        deserializer = [] (Data& d, const std::string& data) {
            d.initialize<T>();
            std::stringstream input(data);
            cereal::BinaryInputArchive iarchive(input);
            iarchive(*reinterpret_cast<T*>(d.ptr.get()));
        };
    }

    template <typename Archive>
    void save(Archive& ar) const {
        ar(serializer(*this)); 
        ar(deserializer);
    }

    template <typename Archive>
    void load(Archive& ar) {
        std::string serialized_data;
        ar(serialized_data);
        ar(deserializer);
        deserializer(*this, serialized_data);
    }

    template <typename T>
    T& get_as() {
        tlassert(ptr != nullptr);
        return *reinterpret_cast<T*>(ptr.get()); 
    }
};

template <typename T>
Data make_data(T&& value) {
    return Data(std::forward<T>(value));
}

inline Data empty_data() {
    return Data();
}

} //end namespace taskloaf

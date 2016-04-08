#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/set.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace taskloaf {

struct Data {
    std::shared_ptr<void> ptr;
    std::string serialized_data;
    std::function<std::string()> serializer;

    Data():
        ptr(nullptr),
        serializer(nullptr)
    {}

    template <typename T>
    Data(T value):
        ptr(
            new T(std::move(value)),
            [] (void* data_ptr) 
            {
                delete reinterpret_cast<T*>(data_ptr);
            }
        ),
        serializer([ptr = this->ptr] () mutable {
            std::stringstream serialized_data;
            cereal::BinaryOutputArchive oarchive(serialized_data);
            oarchive(*reinterpret_cast<T*>(ptr.get()));
            return serialized_data.str();
        })
    {}

    template <typename Archive>
    void save(Archive& ar) const {
        ar(serializer()); 
    }

    template <typename Archive>
    void load(Archive& ar) {
        ar(serialized_data);
        ptr = nullptr;
        serializer = nullptr;
    }

    template <typename T>
    T& get_as() {
        if (ptr == nullptr) {
            ptr = std::make_shared<T>();
            std::stringstream input(serialized_data);
            cereal::BinaryInputArchive iarchive(input);
            iarchive(*reinterpret_cast<T*>(ptr.get()));
        }
        return *reinterpret_cast<T*>(ptr.get()); 
    }
};

template <typename T>
Data make_data(T value) {
    return Data(std::move(value));
}

inline Data empty_data() {
    return Data();
}

} //end namespace taskloaf

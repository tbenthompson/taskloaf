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

struct SafeVoidPtr {
    std::shared_ptr<void> ptr;

    template <typename T>
    SafeVoidPtr(T value):
        ptr(new T(std::move(value)),
            [] (void* data_ptr) 
            {
                delete reinterpret_cast<T*>(data_ptr);
            }
        )
    {}

    template <typename T>
    T& get_as() 
    {
        return *reinterpret_cast<T*>(ptr.get());
    }
};

struct SerializedData {
    std::stringstream stream;

    size_t n_bytes() {
        auto start_pos = stream.tellg();
        stream.seekg(0, std::ios::end);
        auto size = stream.tellg();
        stream.seekg(start_pos);
        return size;
    }
};

struct Data {
    SafeVoidPtr ptr;
    std::function<SerializedData()> serialize;

    Data():
        ptr(nullptr),
        serialize(nullptr)
    {}

    template <typename T>
    Data(T value):
        ptr(std::move(value)),
        serialize([ptr = this->ptr] () mutable {
            std::stringstream serialized_data;
            cereal::BinaryOutputArchive oarchive(serialized_data);
            oarchive(ptr.get_as<T>());
            return SerializedData{std::move(serialized_data)};
        })
    {}

    template <typename T>
    T& get_as() 
    {
        return ptr.get_as<T>();
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

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
    std::shared_ptr<void> ptr;
    std::function<SerializedData()> serializer;

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
            return SerializedData{std::move(serialized_data)};
        })
    {}

    template <typename Archive>
    void serialize(Archive& ar) {
        (void)ar;
    }

    template <typename T>
    T& get_as() { return *reinterpret_cast<T*>(ptr.get()); }

    template <typename T>
    const T& get_as() const { return *reinterpret_cast<T*>(ptr.get()); }
};

template <typename T>
Data make_data(T value) {
    return Data(std::move(value));
}

inline Data empty_data() {
    return Data();
}

} //end namespace taskloaf

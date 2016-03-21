#pragma once

#include <cereal/archives/binary.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace taskloaf {

struct SafeVoidPtr {
    std::shared_ptr<void> ptr;

    template <typename T>
    T& get_as() {
        return *reinterpret_cast<T*>(ptr.get());
    }
};

template <typename T>
SafeVoidPtr make_safe_void_ptr(T value)
{
    return {std::shared_ptr<void>(new T(std::move(value)),
        [] (void* data_ptr) 
        {
            delete reinterpret_cast<T*>(data_ptr);
        }
    )};
}

// 
struct Data {
    SafeVoidPtr unserialized_data;
    std::shared_ptr<std::once_flag> serialized_flag;
    std::shared_ptr<std::stringstream> serialized_data;

    Data(SafeVoidPtr d):
        unserialized_data(d),
        serialized_flag(std::make_shared<std::once_flag>()),
        serialized_data(std::make_shared<std::stringstream>())
    {}

    template <typename T>
    T& get_as() 
    {
        // if (unserialized_data.ptr == nullptr) {
        //     auto data = deserialize<T>(serialized_data);
        //     unserialized_data = make_safe_void_ptr(std::move(data));
        // }
        return unserialized_data.get_as<T>();
    }

    // serialization requirements:
    // any type, multiple archives (at minimum, a binary archive and a 
    // pseudo-archive that measures the size of the object).
    template <typename T>
    void serialize() {
        // use std::call_once to ensure thread safety when mutating the state
        // of the data object.
        std::call_once(*serialized_flag, [this] () {
            if (serialized_data->gcount() > 0) {
                return;
            }
            cereal::BinaryOutputArchive oarchive(*serialized_data);
            oarchive(get_as<T>());
        });
    }
};

template <typename T>
Data make_data(T value) {
    return Data(make_safe_void_ptr(std::move(value)));
}

inline Data empty_data() {
    return Data({nullptr});
}

} //end namespace taskloaf

#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/set.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace taskloaf {

struct SafeVoidPtr {
    std::unique_ptr<void,void(*)(void*)> ptr;

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
        if (ptr == nullptr) {
            // auto data = deserialize<T>(serialized_data);
            // unserialized_data = make_safe_void_ptr(std::move(data));
        }
        return *reinterpret_cast<T*>(ptr.get());
    }
};

struct DataInternals {
    SafeVoidPtr unserialized_data;
    std::function<void(DataInternals&)> serializer;
    std::once_flag serialized_flag;
    std::stringstream serialized_data;

    template <typename T>
    DataInternals(T value):
        unserialized_data(std::move(value)),
        serializer([] (DataInternals& data) {
            cereal::BinaryOutputArchive oarchive(data.serialized_data);
            oarchive(data.unserialized_data.get_as<T>());
        })
    {}
};

struct Data {
    std::shared_ptr<DataInternals> internals; 

    Data():
        internals(nullptr)
    {}

    template <typename T>
    Data(T value):
        internals(std::make_shared<DataInternals>(std::move(value)))
    {}

    template <typename T>
    T& get_as() 
    {
        return internals->unserialized_data.get_as<T>();
    }

    // serialization requirements:
    // any type, multiple archives (at minimum, a binary archive and a 
    // pseudo-archive that measures the size of the object).
    void serialize() {
        // use std::call_once to ensure thread safety when mutating the state
        // of the data object.
        std::call_once(internals->serialized_flag, [this] () {
            if (internals->serialized_data.gcount() > 0) {
                return;
            }
            internals->serializer(*internals);
        });
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

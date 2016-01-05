#pragma once

#include <memory>

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

struct Data {
    // Archive serialized_data;
    SafeVoidPtr unserialized_data;

    template <typename T>
    T& get_as() 
    {
        // if (unserialized_data == nullptr) {
        //     auto data = deserialize<T>(serialized_data);
        //     unserialized_data = make_safe_void_ptr(std::move(data));
        // }
        return unserialized_data.get_as<T>();
    }
};

template <typename T>
Data make_data(T value) {
    return Data{make_safe_void_ptr(std::move(value))};
}

inline Data empty_data() {
    return Data{{nullptr}};
}

} //end namespace taskloaf

#pragma once

#include "debug.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

struct data {
    virtual void save(cereal::BinaryOutputArchive& ar) const = 0;
    virtual void* get_storage() = 0;
    virtual const std::type_info& type() = 0;
};

struct data_ptr {
    std::unique_ptr<data> ptr;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) { 
        (void)ar;
    }

    template <typename T>
    T& get() {
        TLASSERT(ptr->type() == typeid(T));
        return *reinterpret_cast<T*>(ptr->get_storage());
    }

    template <typename T>
    operator T&() {
        return get<T>();
    }
};

template <typename T>
struct typed_data: public data {
    T v;

    typed_data(T& v): v(v) {}
    typed_data(T&& v): v(std::move(v)) {}

    void* get_storage() {
        return &v;
    }

    const std::type_info& type() {
        return typeid(T);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(v);
    }
};

template <typename T>
data_ptr ensure_data(T&& v) {
    return {std::make_unique<typed_data<std::decay_t<T>>>(std::forward<T>(v))};
}

inline data_ptr ensure_data() {
    return {std::make_unique<typed_data<bool>>(false)};
}

} //end namespace taskloaf

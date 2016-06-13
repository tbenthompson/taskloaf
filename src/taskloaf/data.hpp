#pragma once

#include "debug.hpp"
#include "fnc_registry.hpp"

#include <cereal/archives/binary.hpp>

namespace taskloaf {

struct ignore {};

struct data {
    virtual ~data() {};
    virtual void save(cereal::BinaryOutputArchive& ar) const = 0;
    virtual void* get_storage() = 0;
    virtual const std::type_info& type() = 0;
};

struct data_ptr {
    using deserializer_type = void(*)(data_ptr&,cereal::BinaryInputArchive&);
    std::shared_ptr<data> ptr;

    bool empty() {
        return ptr == nullptr;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ptr->save(ar);
    }

    void load(cereal::BinaryInputArchive& ar) { 
        std::pair<size_t,size_t> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<deserializer_type>(
            get_fnc_registry().get_function(deserializer_id)
        );
        deserializer(*this, ar);
    }


    template <typename T>
    T& get() {
        TLASSERT(ptr->type() == typeid(T) || typeid(ignore) == typeid(T));
        return *reinterpret_cast<T*>(ptr->get_storage());
    }

    template <typename T>
    operator T&() { return get<T>(); }
};

template <typename T, 
    std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
void data_save(T& v, cereal::BinaryOutputArchive& ar) {
    ar(v);
}

template <typename T, 
    std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
void data_save(T& v, cereal::BinaryOutputArchive& ar) {
    ar.saveBinary(&v, sizeof(T));
}

template <typename T, 
    std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
std::unique_ptr<data> data_load(cereal::BinaryInputArchive& ar);

template <typename T, 
    std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
std::unique_ptr<data> data_load(cereal::BinaryInputArchive& ar);

template <typename T>
struct typed_data: public data {
    T v;

    typed_data() = default;
    typed_data(T& v): v(v) {}
    typed_data(T&& v): v(std::move(v)) {}

    T& get() {
        return v;
    }

    void* get_storage() {
        return &v;
    }

    const std::type_info& type() {
        return typeid(T);
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        auto d = [] (data_ptr& out, cereal::BinaryInputArchive& ar) {
            out.ptr = data_load<T>(ar);
        };
        auto deserializer_id = register_fnc<
            decltype(d),void,data_ptr&,cereal::BinaryInputArchive&
        >::add_to_registry();
        ar(deserializer_id);

        data_save(v, ar);
    }
};

template <typename T, 
    std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
std::unique_ptr<data> data_load(cereal::BinaryInputArchive& ar) {
    auto v_ptr = std::make_unique<typed_data<T>>();
    ar((*v_ptr).v);
    return v_ptr;
}

template <typename T, 
    std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
std::unique_ptr<data> data_load(cereal::BinaryInputArchive& ar) {
    std::array<uint8_t,sizeof(T)> v;
    ar.loadBinary(&v, sizeof(T));
    return std::make_unique<typed_data<T>>(std::move(*reinterpret_cast<T*>(&v)));
}

template <typename T>
data_ptr make_data(T&& v) {
    return {std::make_unique<typed_data<std::decay_t<T>>>(std::forward<T>(v))};
}

} //end namespace taskloaf

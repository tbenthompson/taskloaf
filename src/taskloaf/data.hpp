#pragma once
#include "tlassert.hpp"

#include <cereal/archives/binary.hpp>

#include <mapbox/variant.hpp>

namespace taskloaf {

template <typename F>
struct is_serializable: std::integral_constant<bool,
    cereal::traits::is_output_serializable<F,cereal::BinaryOutputArchive>::value &&
    cereal::traits::is_input_serializable<F,cereal::BinaryInputArchive>::value &&
    !std::is_pointer<F>::value> {};

template <typename T>
struct Data {
    static_assert(
        is_serializable<std::decay_t<T>>::value,
        "Types encapsulated in Data must be serializable."
    );
    
    mapbox::util::variant<T,std::shared_ptr<T>> v;

    Data() = default;
    Data(const T& v): v(v) {}
    Data(T&& v): v(std::move(v)) {}

    Data(Data<T>&& other) = default;
    Data& operator=(Data<T>&& other) = default;

    Data(const Data<T>& other) { *this = other; }
    Data& operator=(const Data<T>& other) {
        const_cast<Data<T>*>(&other)->promote();
        v = other.v;
        return *this;
    }

    void promote() {
        if (v.which() != 0) {
            return;
        }
        v = std::make_shared<T>(std::move(v.template get_unchecked<T>()));
    }

    operator T&() {
        if (v.which() == 0) {
            return v.template get_unchecked<T>();
        } else {
            auto& ptr = v.template get_unchecked<std::shared_ptr<T>>();
            tlassert(ptr != nullptr);
            return *ptr;
        }
    }

    operator const T&() const { return *const_cast<Data<T>*>(this); }

    T& get() { return *this; }
    const T& get() const { return *this; }

    void save(cereal::BinaryOutputArchive& ar) const { ar(get()); }
    void load(cereal::BinaryInputArchive& ar) { ar(get()); }
};

template <typename T>
auto make_data(T&& v) {
    return Data<std::decay_t<T>>(std::forward<T>(v));
}

template <typename T>
struct IsData: std::false_type {};

template <typename T>
struct IsData<Data<T>>: std::true_type {};

template <typename T>
struct EnsureData {
    using type = Data<T>;
};

template <typename T>
struct EnsureData<Data<T>> {
    using type = Data<T>;
};

template <typename T>
using EnsureDataT = typename EnsureData<T>::type;

template <typename T, std::enable_if_t<!IsData<T>::value>* = nullptr>
auto ensure_data(T&& v) {
    return make_data(std::forward<T>(v));
}

template <typename T, std::enable_if_t<IsData<T>::value>* = nullptr>
auto ensure_data(T&& v) {
    return std::forward<T>(v);
}

} //end namespace taskloaf

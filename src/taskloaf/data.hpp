#pragma once

#include "fnc.hpp"

#include <eggs/variant.hpp>

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

    using VariantT = eggs::variant<T,std::shared_ptr<T>>;

    VariantT val;

    Data() = default;
    Data(Data<T>&& other) = default;
    Data& operator=(Data<T>&& other) = default;

    Data(const Data<T>& other) {
        *this = other;
    }
    Data& operator=(const Data<T>& other) {
        const_cast<Data<T>*>(&other)->promote();
        val = other.val;
        return *this;
    }

    template <typename U,
        std::enable_if_t<
            !std::is_same<std::decay_t<U>,Data<T>>::value
        >* = nullptr>
    Data(U&& v): val(std::forward<U>(v)) {}

    void promote() {
        if (val.which() != 0) {
            return;
        }
        val = std::make_shared<T>(std::move(get()));
    }

    bool operator==(std::nullptr_t) const {
        return val.which() == VariantT::npos;
    }

    bool operator!=(std::nullptr_t) const {
        return !(*this == nullptr);
    }

    T& get() {
        tlassert(*this != nullptr);
        return *this;
    }

    const T& get() const {
        return *this;
    }

    operator T&() {
        if (val.which() == 0) {
            return *val.template target<T>();
        } else {
            return **val.template target<std::shared_ptr<T>>();
        }
    }

    operator const T&() const {
        T& out = *const_cast<Data<T>*>(this);
        return out;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        bool is_null = *this == nullptr;
        ar(is_null);
        if (!is_null) {
            ar(get());
        }
    }

    void load(cereal::BinaryInputArchive& ar) {
        tlassert(*this == nullptr);
        bool is_null;
        ar(is_null);
        if (!is_null) {
            val.template emplace<0>();
            ar(get());
        }
    }
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

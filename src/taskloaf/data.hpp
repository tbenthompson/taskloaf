#pragma once

#include "fnc.hpp"

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
    
    union {
        T val;
        std::shared_ptr<T> ptr;
    };
    int which = 0;

    Data(): val() {}
    Data(const T& v): val(v) {}
    Data(T&& v): val(std::move(v)) {}

    Data(Data<T>&& other):
        val()
    {
        *this = std::move(other);
    }

    Data& operator=(Data<T>&& other) {
        this->~Data();    
        which = other.which;
        if (which == 0) {
            new (&val) T(std::move(other.val));
        } else {
            new (&ptr) std::shared_ptr<T>(std::move(other.ptr));
        }
        return *this;
    }

    Data(const Data<T>& other) {
        *this = other;
    }
    Data& operator=(const Data<T>& other) {
        const_cast<Data<T>*>(&other)->promote();
        this->~Data();
        which = 1;
        new (&ptr) std::shared_ptr<T>(other.ptr);
        return *this;
    }

    ~Data() {
        if (which == 0) {
            val.~T();
        } else if (which == 1) {
            ptr.~shared_ptr();
        }
    }

    void promote() {
        if (which != 0) {
            return;
        }
        T tmp_val = std::move(val);
        val.~T();
        which = 1;
        new (&ptr) std::shared_ptr<T>(std::make_shared<T>(std::move(tmp_val)));
    }

    T& get() {
        return *this;
    }

    const T& get() const {
        return *this;
    }

    operator T&() {
        if (which == 0) {
            return val;
        } else {
            tlassert(ptr != nullptr);
            return *ptr;
        }
    }

    operator const T&() const {
        T& out = *const_cast<Data<T>*>(this);
        return out;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(get());
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(val);
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

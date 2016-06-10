// This is based off of https://github.com/david-grs/static_any which is
// licensed as follows. 
// 
// The MIT License (MIT)
// 
// Copyright (c) 2015 david-grs
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// I started implementing taskloaf with templates throughout since I had
// assumed that was necessary to get fast stack allocated futures. However...
// I'm a big fan of quick compiles. So, when I was faced with the absurd
// compile times that resulted, I backtracked. How to store any type without
// templates. Well, boost::any (soon, std::experimental::any), are useful. But,
// they don't do stack allocation... 
//
// different any implementations:
// boost::any
// QVariant
// cdiggins::any -- lots of problems here...
// static_any (https://github.com/david-grs/static_any, http://david-grs.github.io/low_latency_stack_based_boost_any/)
// static_any_t (https://github.com/david-grs/static_any)
// std::experimental::variant 
//
// static_any uses the stack for anything less a certain size and fails above 
// that
//
// taskloaf has different requirements from all these cases. First off, the
// data needs to be serializable, so a custom any implementation is necessary,
// or at least a wrapper that reimplements significant portions of the behavior.
// Second, the data is considered immutable, which means that either value or
// reference semantics are acceptable, depending on which is more efficient. 
// Third, similar to static_any, I would like to use stack storage in situations
// where value semantics is the desired behavior.
// As a result, the goal is to template the any type on a 
//
// For IVarData: two problems,  
// small vector optimization for the trigger list?
// https://howardhinnant.github.io/stack_alloc.html
// pool allocation?

#pragma once

#include "debug.hpp"
#include "fnc_registry.hpp"

#include <cereal/archives/binary.hpp>

#include <cstring>

namespace taskloaf {

template <size_t N, bool TC>
struct sized_any {};

template <size_t N>
struct sized_any<N,false> {
    constexpr static size_t max_size = N;

    enum class action { get_type, copy, move, destroy, serialize };

    using deserializer_type = void(*)(sized_any<N,false>&,cereal::BinaryInputArchive&);
    using storage_type = std::aligned_storage_t<max_size,alignof(std::max_align_t)>;
    using policy_type = void(*)(action,sized_any<N,false>*,void*);

    storage_type storage;
    policy_type policy = nullptr;

    sized_any() = default;
        
    sized_any(const sized_any& other) {
        copy_from(other);
    }

    sized_any& operator=(const sized_any& other) {
        clear();
        copy_from(other);
        return *this;
    }

    sized_any(sized_any&& other) {
        move_from(std::move(other));
    }

    sized_any& operator=(sized_any&& other) {
        clear();
        move_from(std::move(other));
        return *this;
    }

    void copy_from(const sized_any& other) {
        policy = other.policy;
        if (!empty()) {
            auto other_ptr = const_cast<void*>(reinterpret_cast<const void*>(&other));
            policy(action::copy, this, other_ptr);
        }
    }

    void move_from(sized_any&& other) {
        policy = other.policy;
        if (!empty()) {
            policy(action::move, this, &other);
        }
    }

    ~sized_any() {
        clear();
    }

    void clear() { 
        if (!empty()) {
            policy(action::destroy, this, nullptr);
            policy = nullptr;
        }
    }

    bool empty() const {
        return policy == nullptr;
    }

    const std::type_info& type() const {
        if (empty()) {
            return typeid(void);
        }
        const std::type_info* out;
        policy(action::get_type, const_cast<sized_any<N,false>*>(this), &out);
        return *out;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(empty());
        if (!empty()) {
            policy(action::serialize, const_cast<sized_any<N,false>*>(this), &ar);
        }
    }

    void load(cereal::BinaryInputArchive& ar) {
        clear();

        bool is_empty;
        ar(is_empty);
        if (!is_empty) {
            std::pair<size_t,size_t> deserializer_id;
            ar(deserializer_id);
            auto deserializer = reinterpret_cast<deserializer_type>(
                get_fnc_registry().get_function(deserializer_id)
            );
            deserializer(*this, ar);
        }
    }

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,sized_any<N,false>>::value
        >* = nullptr>
    explicit sized_any(T&& v) {
        using DecayT = std::decay_t<T>;
        static_assert(sizeof(DecayT) <= N, "T is too big to fit");

        policy = &policy_fnc<DecayT>;
        new (&storage) DecayT(std::forward<T>(v));
    }

    template <typename T>
    T& unchecked_get() {
        return *reinterpret_cast<T*>(&storage);
    }

    template <typename T>
    T& get() {
        TLASSERT(type() == typeid(T));
        return unchecked_get<T>();
    }

    template <typename T>
    operator T&() {
        return get<T>();
    }

    template <typename T>
    static void policy_fnc(action action, sized_any<N,false>* any_ptr, void* ptr2) {
        T* obj_ptr = reinterpret_cast<T*>(&any_ptr->storage);

        switch(action) {
        case sized_any<N,false>::action::get_type: {
            *reinterpret_cast<const std::type_info**>(ptr2) = &typeid(T);
            break;
        } case sized_any<N,false>::action::copy: {
            auto* other_any_ptr = reinterpret_cast<sized_any<N,false>*>(ptr2);
            TLASSERT(other_any_ptr != nullptr);
            auto& other_obj = other_any_ptr->get<T>();

            TLASSERT(obj_ptr != nullptr);
            new (obj_ptr) T(other_obj);
            break;
        } case sized_any<N,false>::action::move: {
            auto* other_any_ptr = reinterpret_cast<sized_any<N,false>*>(ptr2);
            TLASSERT(other_any_ptr != nullptr);
            auto& other_obj = other_any_ptr->get<T>();

            TLASSERT(obj_ptr != nullptr);
            new (obj_ptr) T(std::move(other_obj));
            break;
        } case sized_any<N,false>::action::destroy: {
            TLASSERT(obj_ptr != nullptr);
            obj_ptr->~T();
            break;
        } case sized_any<N,false>::action::serialize: {
            TLASSERT(obj_ptr != nullptr);
            auto ar = *reinterpret_cast<cereal::BinaryOutputArchive*>(ptr2);
            auto deserializer =
                [] (sized_any<N,false>& a, cereal::BinaryInputArchive& ar) {
                    T obj;
                    ar(obj);
                    a = sized_any<N,false>(std::move(obj));
                };
            auto deserializer_id = register_fnc<
                decltype(deserializer),void,
                sized_any<N,false>&,cereal::BinaryInputArchive&
            >::add_to_registry();
            ar(deserializer_id);
            ar(*obj_ptr);
        }
        }
    }
};

template <size_t N>
struct sized_any<N,true> {
    constexpr static size_t max_size = N;

    using storage_type = std::aligned_storage_t<max_size,alignof(std::max_align_t)>;

    storage_type storage;

    sized_any() = default;

    void save(cereal::BinaryOutputArchive& ar) const {
        ar.saveBinary(&storage, N);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar.loadBinary(&storage, N);
    }

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,sized_any<N,true>>::value
        >* = nullptr>
    explicit sized_any(T&& v) {
        using DecayT = std::decay_t<T>;
        static_assert(sizeof(DecayT) <= N, "T is too big to fit");
        std::memcpy(&storage, &v, sizeof(DecayT));
    }

    template <typename T>
    T& unchecked_get() {
        return *reinterpret_cast<T*>(&storage);
    }

    template <typename T>
    T& get() {
        return unchecked_get<T>();
    }

    template <typename T>
    operator T&() {
        return get<T>();
    }
};

template <size_t N>
constexpr size_t sized_any<N,false>::max_size;
template <size_t N>
constexpr size_t sized_any<N,true>::max_size;

#define TL_N_ANY_SIZES 8

constexpr size_t size_ranges[TL_N_ANY_SIZES+1] = {
    0, 4, 8, 16, 24, 32, 128, 512, 2048
};

#define FOREACH_ANY_SIZE(DO, ...)\
    DO(0, __VA_ARGS__)\
    DO(1, __VA_ARGS__)\
    DO(2, __VA_ARGS__)\
    DO(3, __VA_ARGS__)\
    DO(4, __VA_ARGS__)\
    DO(5, __VA_ARGS__)\
    DO(6, __VA_ARGS__)\
    DO(7, __VA_ARGS__)

template <size_t N, size_t L, size_t U>
using upper_inclusive_in_range = std::integral_constant<bool, L < N && N <= U>;

template <size_t N, size_t L, size_t U>
constexpr bool upper_inclusive_in_range_v = upper_inclusive_in_range<N,L,U>::value;

template <typename T>
struct is_sized_any: std::false_type {};
template <size_t I, bool TC>
struct is_sized_any<sized_any<I,TC>>: std::true_type {};

#define ENSURE_ANY(I,UNUSED) \
    template <typename T, \
        std::enable_if_t<\
            upper_inclusive_in_range_v<\
                sizeof(std::decay_t<T>),size_ranges[I],size_ranges[I+1]>\
            && !is_sized_any<std::decay_t<T>>::value\
        >* = nullptr>\
    auto ensure_any(T&& v) {\
        return sized_any<\
            size_ranges[I+1],\
            std::is_trivially_copyable<std::decay_t<T>>::value\
        >(std::forward<T>(v)); \
    }

FOREACH_ANY_SIZE(ENSURE_ANY)

template <typename T, 
    std::enable_if_t<is_sized_any<std::decay_t<T>>::value>* = nullptr>
auto ensure_any(T&& v) {
    return std::forward<T>(v); 
}

inline auto ensure_any() { return sized_any<4,true>{}; }

template <typename T,
    std::enable_if_t<(sizeof(std::decay_t<T>) > size_ranges[TL_N_ANY_SIZES])>* = nullptr>
auto ensure_any(T&&) {
    static_assert(sizeof(T) < size_ranges[TL_N_ANY_SIZES], 
        "T is too big to use sized_any, please wrap it in a pointer.");
}

} // end namespace taskloaf

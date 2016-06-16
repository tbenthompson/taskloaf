#pragma once

#include "debug.hpp"
#include "fnc_registry.hpp"
#include "worker.hpp"

#include <cereal/archives/binary.hpp>

#include <boost/pool/pool.hpp>

namespace taskloaf {

struct ignore {};
using _ = ignore;

template <size_t AllocSize>
inline boost::pool<>& get_pool() {
    static thread_local boost::pool<> pool(AllocSize);
    return pool;
}

enum class action {
    copy,
    get_type,
    serialize,
    destroy
};

struct data {
    using deserializer_type = data(*)(cereal::BinaryInputArchive&);
    using policy_type = void(*)(action,data*,void*);

    static constexpr size_t internal_storage_size = sizeof(void*);

    template <typename T>
    static constexpr bool store_directly_v = 
        std::is_trivially_copyable<T>::value && (sizeof(T) < internal_storage_size);
    
    union {
        void* ptr;
        std::aligned_storage_t<internal_storage_size> storage;
    };
    TL_IF_DEBUG(boost::pool<>* create_pool;,)
    policy_type policy = nullptr;
    bool is_ptr;

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,data>::value && 
            !store_directly_v<std::decay_t<T>>
        >* = nullptr>
    data(T&& v) {
        is_ptr = true;
        policy = policy_fnc<std::decay_t<T>>;
        setup(std::forward<T>(v));
    }

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,data>::value && 
            store_directly_v<std::decay_t<T>>
        >* = nullptr>
    data(T&& v) {
        is_ptr = false;
        policy = policy_fnc<std::decay_t<T>>;
        std::memcpy(&storage, &v, sizeof(void*));
    }

    data() = default;

    data(const data& other) {
        copy_from(other);
    }

    data& operator=(const data& other) {
        destroy();
        copy_from(other);
        return *this;
    }

    data(data&& other) {
        move_from(std::move(other));
    }

    data& operator=(data&& other) {
        destroy();
        move_from(std::move(other));
        return *this;
    }

    ~data() {
        destroy();
    }
    
    void destroy() {
        if (!empty() && is_ptr) {
            policy(action::destroy, this, nullptr);
        }
    }

    void set_policy(const data& other) {
        policy = other.policy;
        is_ptr = other.is_ptr;
    }

    void copy_from(const data& other) {
        set_policy(other);
        if (!empty()) {
            if (other.is_ptr) {
                policy(action::copy, this, const_cast<data*>(&other));
            } else {
                std::memcpy(&storage, &other.storage, sizeof(internal_storage_size));
            }
        }
    }

    void move_from(data&& other) {
        set_policy(other);
        if (!empty()) {
            TL_IF_DEBUG(create_pool = other.create_pool;,)
            std::memcpy(&storage, &other.storage, sizeof(internal_storage_size));
            other.policy = nullptr;
        }
    }

    bool empty() const {
        return policy == nullptr; 
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        policy(action::serialize, const_cast<data*>(this), &ar);
    }

    void load(cereal::BinaryInputArchive& ar) { 
        std::pair<size_t,size_t> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<deserializer_type>(
            get_fnc_registry().get_function(deserializer_id)
        );
        *this = deserializer(ar);
    }

    template <typename T, std::enable_if_t<!store_directly_v<T>>* = nullptr>
    T& unchecked_get() {
        return *reinterpret_cast<T*>(ptr);
    }

    template <typename T, std::enable_if_t<store_directly_v<T>>* = nullptr>
    T& unchecked_get() {
        return *reinterpret_cast<T*>(&storage);
    }

    template <typename T>
    T& get() {
        TL_IF_DEBUG(
        const std::type_info* t;
        policy(action::get_type, this, &t);
        bool type_check = *t == typeid(T) || typeid(ignore) == typeid(T);
        if (!(type_check)) {
            std::cout << "Runtime type check failed: " << std::endl;
            std::cout << t->name() << " " << typeid(T).name() << std::endl;
        }
        ,)
        TLASSERT(type_check);
        return unchecked_get<T>();
    }

    template <typename T>
    const T& get() const { return const_cast<data*>(this)->template get<T>(); }

    template <typename T>
    operator T&() { return get<T>(); }

    template <typename T>
    void setup(T&& v) {
        using stored_type = std::decay_t<T>;
        auto& pool = get_pool<sizeof(stored_type)>();
        ptr = pool.malloc();
        TL_IF_DEBUG(create_pool = &pool;,)
        new (ptr) stored_type(std::forward<T>(v));
    }

    template <typename T, 
        std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
    static void data_save(T& v, cereal::BinaryOutputArchive& ar) {
        ar(v);
    }

    template <typename T, 
        std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
    static void data_save(T& v, cereal::BinaryOutputArchive& ar) {
        ar.saveBinary(&v, sizeof(T));
    }

    template <typename T, 
        std::enable_if_t<!std::is_trivially_copyable<T>::value>* = nullptr>
    static data data_load(cereal::BinaryInputArchive& ar) {
        T v;
        ar(v);
        return data(std::move(v));
    }

    template <typename T, 
        std::enable_if_t<std::is_trivially_copyable<T>::value>* = nullptr>
    static data data_load(cereal::BinaryInputArchive& ar) {
        std::array<uint8_t,sizeof(T)> v;
        ar.loadBinary(&v, sizeof(T));
        return data(std::move(*reinterpret_cast<T*>(&v)));
    }

    template <typename T>
    static void serialize(data* any_ptr, void* ptr2) {
        auto& ar = *reinterpret_cast<cereal::BinaryOutputArchive*>(ptr2);

        auto d = [] (cereal::BinaryInputArchive& ar) {
            return data_load<T>(ar);
        };
        auto deserializer_id = register_fnc<
            decltype(d),data,cereal::BinaryInputArchive&
        >::add_to_registry();
        ar(deserializer_id);
        
        data_save<T>(any_ptr->template get<T>(), ar);
    }

    template <typename T>
    static void get_type(void* ptr2) {
        *reinterpret_cast<const std::type_info**>(ptr2) = &typeid(T);
    }

    template <typename T, std::enable_if_t<!store_directly_v<T>>* = nullptr>
    static void policy_fnc(action a, data* d_ptr, void* ptr2) {
        switch(a) {
        case action::copy: { 
            auto& other_val = reinterpret_cast<data*>(ptr2)->get<T>();
            d_ptr->setup(other_val);
            break;
        }
        case action::destroy: {
            d_ptr->get<T>().~T();
            auto& pool = get_pool<sizeof(T)>();
            // Check that the pointer is being deleted with the same
            // memory pool that allocated it.
            TLASSERT(d_ptr->create_pool == &pool);
            pool.free(d_ptr->ptr);
            break;
        }
        case action::get_type: { get_type<T>(ptr2); break; }
        case action::serialize: { serialize<T>(d_ptr, ptr2); break; }
        }
    }

    template <typename T, std::enable_if_t<store_directly_v<T>>* = nullptr>
    static void policy_fnc(action a, data* d_ptr, void* ptr2) {
        switch(a) {
        case action::get_type: { get_type<T>(ptr2); break; }
        case action::serialize: { serialize<T>(d_ptr, ptr2); break; }
        // These shouldn't be called for a trivially copyable type
        // since memcpy can be used and the destructor is trivial.
        case action::copy: { TLASSERT(false); break; }
        case action::destroy: { TLASSERT(false); break; }
        }
    }
};

} //end namespace taskloaf

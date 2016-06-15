#pragma once

#include "debug.hpp"
#include "fnc_registry.hpp"
#include "worker.hpp"

#include <cereal/archives/binary.hpp>

#include <boost/pool/pool.hpp>

namespace taskloaf {

struct ignore {};
using _ = ignore;

struct data_allocator {
    size_t n_bytes;

    data_allocator(size_t n_bytes): n_bytes(n_bytes) {}

    void* malloc() {
        return new uint8_t[n_bytes];
    }

    void free(void* ptr) {
        delete[] reinterpret_cast<uint8_t*>(ptr);
    }
};

// template <size_t AllocSize>
// static boost::pool<>& get_pool() {
//     thread_local boost::pool<> pool(AllocSize);
//     return pool;
// }

template <size_t AllocSize>
static data_allocator& get_pool() {
    static data_allocator pool(AllocSize);
    return pool;
}

enum class action {
    get_type,
    serialize,
    destroy
};

template <typename T>
struct data_internal {
    size_t ref_count;
    address owner;
    T val;
};

struct data {
    using deserializer_type = data(*)(cereal::BinaryInputArchive&);
    using policy_type = void(*)(action,data*,void*);

    static constexpr size_t InternalStorage = sizeof(void*);

    template <typename T>
    static constexpr bool store_directly_v = 
        std::is_trivially_copyable<T>::value && (sizeof(T) < InternalStorage);
    
    union {
        void* ptr;
        std::aligned_storage_t<InternalStorage> storage;
    };
    policy_type policy = nullptr;
    bool is_ptr;

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,data>::value && 
            !store_directly_v<std::decay_t<T>>
        >* = nullptr>
    data(T&& v) {
        using stored_type = data_internal<std::decay_t<T>>;
        constexpr size_t alloc_size = sizeof(stored_type);

        is_ptr = true;
        ptr = get_pool<alloc_size>().malloc();
        new (ptr) stored_type{1, cur_addr, std::forward<T>(v)};
        policy = policy_fnc<std::decay_t<T>>;

        TLASSERT(get_references() == 1); 
    }

    template <typename T,
        std::enable_if_t<
            !std::is_same<std::decay_t<T>,data>::value && 
            store_directly_v<std::decay_t<T>>
        >* = nullptr>
    data(T&& v) {
        is_ptr = false;
        std::memcpy(&storage, &v, sizeof(void*));
        policy = policy_fnc<std::decay_t<T>>;
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
            get_references()--;
            if (get_references() == 0) {
                policy(action::destroy, this, nullptr);
            }
        }
    }

    void set_policy(const data& other) {
        policy = other.policy;
        is_ptr = other.is_ptr;
    }

    void memcpy_internal(const data& other) {
        if (other.is_ptr) {
            std::memcpy(&ptr, &other.ptr, sizeof(void*));
        } else {
            std::memcpy(&storage, &other.storage, sizeof(InternalStorage));
        }
    }

    void copy_from(const data& other) {
        set_policy(other);
        if (!empty()) {
            memcpy_internal(other);
            if (is_ptr) {
                get_references()++;
            }
        }
    }

    void move_from(data&& other) {
        set_policy(other);
        if (!empty()) {
            memcpy_internal(other);
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
    T* unchecked_get_ptr() {
        return &reinterpret_cast<data_internal<T>*>(ptr)->val;
    }

    template <typename T, std::enable_if_t<store_directly_v<T>>* = nullptr>
    T* unchecked_get_ptr() {
        return reinterpret_cast<T*>(&storage);
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
        return *unchecked_get_ptr<T>();
    }

    template <typename T>
    const T& get() const { return const_cast<data*>(this)->template get<T>(); }

    template <typename T>
    operator T&() { return get<T>(); }

    size_t& get_references() {
        return reinterpret_cast<data_internal<ignore>*>(ptr)->ref_count;
    }

    address& get_owner() {
        return reinterpret_cast<data_internal<ignore>*>(ptr)->owner;
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
    static void policy_fnc(action a, data* any_ptr, void* ptr2) {
        switch(a) {
        case action::get_type: { get_type<T>(ptr2); break; }
        case action::serialize: { serialize<T>(any_ptr, ptr2); break; }
        case action::destroy: {
            constexpr static size_t obj_size = sizeof(data_internal<T>);

            any_ptr->template unchecked_get_ptr<T>()->~T();

            if (any_ptr->get_owner() == cur_addr) {
                get_pool<obj_size>().free(any_ptr->ptr);
            } else {
                cur_worker->add_task(any_ptr->get_owner(), closure(
                    [] (void* ptr,_) {
                        get_pool<obj_size>().free(ptr); 
                        return _{};
                    },
                    any_ptr->ptr
                ));
            }
            break;
        }
        }
    }

    template <typename T, std::enable_if_t<store_directly_v<T>>* = nullptr>
    static void policy_fnc(action a, data* any_ptr, void* ptr2) {
        switch(a) {
        case action::get_type: { get_type<T>(ptr2); break; }
        case action::serialize: { serialize<T>(any_ptr, ptr2); break; }
        case action::destroy: { break; }
        }
    }
};

} //end namespace taskloaf

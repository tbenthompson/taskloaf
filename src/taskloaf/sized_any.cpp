#include "sized_any.hpp"

namespace taskloaf {

template <size_t N>
sized_any<N,false>::sized_any() = default;
    
template <size_t N>
sized_any<N,false>::sized_any(const sized_any& other) {
    copy_from(other);
}

template <size_t N>
sized_any<N,false>& sized_any<N,false>::operator=(const sized_any& other) {
    clear();
    copy_from(other);
    return *this;
}

template <size_t N>
sized_any<N,false>::sized_any(sized_any&& other) {
    move_from(std::move(other));
}

template <size_t N>
sized_any<N,false>& sized_any<N,false>::operator=(sized_any&& other) {
    clear();
    move_from(std::move(other));
    return *this;
}

template <size_t N>
void sized_any<N,false>::copy_from(const sized_any& other) {
    policy = other.policy;
    if (!empty()) {
        auto other_ptr = const_cast<void*>(reinterpret_cast<const void*>(&other));
        policy(action::copy, this, other_ptr);
    }
}

template <size_t N>
void sized_any<N,false>::move_from(sized_any&& other) {
    policy = other.policy;
    if (!empty()) {
        policy(action::move, this, &other);
    }
}

template <size_t N>
sized_any<N,false>::~sized_any() {
    clear();
}

template <size_t N>
void sized_any<N,false>::clear() { 
    if (!empty()) {
        policy(action::destroy, this, nullptr);
        policy = nullptr;
    }
}

template <size_t N>
bool sized_any<N,false>::empty() const {
    return policy == nullptr;
}

template <size_t N>
const std::type_info& sized_any<N,false>::type() const {
    if (empty()) {
        return typeid(void);
    }
    const std::type_info* out;
    policy(action::get_type, const_cast<sized_any<N,false>*>(this), &out);
    return *out;
}

template <size_t N>
void sized_any<N,false>::save(cereal::BinaryOutputArchive& ar) const {
    ar(empty());
    if (!empty()) {
        policy(action::serialize, const_cast<sized_any<N,false>*>(this), &ar);
    }
}

template <size_t N>
void sized_any<N,false>::load(cereal::BinaryInputArchive& ar) {
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

template <size_t N>
sized_any<N,true>::sized_any() = default;

template <size_t N>
void sized_any<N,true>::save(cereal::BinaryOutputArchive& ar) const {
    ar.saveBinary(&storage, N);
}

template <size_t N>
void sized_any<N,true>::load(cereal::BinaryInputArchive& ar) {
    ar.loadBinary(&storage, N);
}

#define INSTANTIATE_ANY(I)\
    template struct sized_any<size_ranges[I+1],false>;\
    template struct sized_any<size_ranges[I+1],true>;

FOREACH_ANY_SIZE(INSTANTIATE_ANY)

}

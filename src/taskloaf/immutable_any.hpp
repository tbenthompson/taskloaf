#pragma once

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
// Second, the data is considered immutable. Third, similar to static_any, I
// would like stack storage.
//

// For IVarData: two problems,  
// first, get rid of multiple return values
// small vector optimization for the trigger list?
// https://howardhinnant.github.io/stack_alloc.html
// pool allocation?

namespace taskloaf {

// need: 
// default constructible
// destructible
// serializable
// 
// struct iany {
//     using get_ptr_type = (void*)(*)();
//     using serializer_type = void (*)(const iany&,cereal::BinaryOutputArchive&);
//     using deserializer_type = void (*)(iany&,cereal::BinaryOutputArchive&);
//     constexpr size_t N_small = 16;
// 
//     typename std::aligned_storage<N_small>::type data;
// 
//     void save(cereal::BinaryOutputArchive& ar) const;
//     void load(cereal::BinaryInputArchive& ar);
// 
//     template <typename T>
//     T& unchecked_cast() {
//         return *reinterpret_cast<T*>(ptr.get()); 
//     }
// 
//     template <typename T>
//     T& cast() {
//         // In debug builds, do a runtime check to make sure the data type is
//         // correct.
//         tlassert(&serialize == &serialize_fnc<T>);
//     }
// };

}

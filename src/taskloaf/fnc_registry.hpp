#pragma once
#include "tlassert.hpp"

#include <memory>
#include <utility>
#include <cereal/types/utility.hpp>

namespace taskloaf {

// A function registry for retrieving functions at runtime. Registry keys
// are a pair of integers. The registry should work across any executables
// that are ABI compatible. So far, I have only tested using idential 
// exectuables. However, I think that two machines compiling ABI-compatible C++
// with ABI-compatible (clang and gcc for examples) should work. This is
// worth further-testing at some point.
struct registry_impl;
struct fnc_registry {
    std::unique_ptr<registry_impl> impl;
    
    fnc_registry();
    ~fnc_registry();

    void insert(const std::type_info& t_info, void* f_ptr);
    std::pair<size_t,size_t> lookup_location(const std::type_info& t_info );
    void* get_function(const std::pair<size_t,size_t>& loc);
};

// Return a fnc_registry with static lifetime. Despite being used from 
// multiple threads, this is thread safe because the construction happens 
// only on the main thread before main() is run (static construction time).
fnc_registry& get_fnc_registry();

// Calling the static function register_fnc<Func,...>::add_to_registry() anywhere
// within the code will result in the Func function type getting added to
// the fnc_registry.
template<typename Func, typename Return, typename... Args>
struct register_fnc
{
    register_fnc() {
        get_fnc_registry().insert(typeid(Func), reinterpret_cast<void*>(call));
    }

    static Return call(Args... args) {
        static int data = 0;
        auto& closure = *reinterpret_cast<Func*>(&data);
        return closure(std::forward<Args>(args)...);
    }

    // When instance is constructed, Func is inserted into the registry.
    static register_fnc instance;

    static std::pair<size_t,size_t> add_to_registry() {
        (void)instance;
        return get_fnc_registry().lookup_location(typeid(Func));
    }
};

template<typename Func, typename Return, typename... Args>
register_fnc<Func,Return,Args...> register_fnc<Func,Return,Args...>::instance;

} //end namespace taskloaf

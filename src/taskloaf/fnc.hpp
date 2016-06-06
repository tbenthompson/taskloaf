#pragma once
#include "tlassert.hpp"

#include "get_signature.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>

#include <memory>

namespace taskloaf {

struct RegistryImpl;
struct FncRegistry {
    std::unique_ptr<RegistryImpl> impl;
    
    FncRegistry();
    ~FncRegistry();

    void insert(const std::type_info& t_info, void* f_ptr);
    std::pair<int,int> lookup_location(const std::type_info& t_info );
    void* get_function(const std::pair<int,int>& loc);
};
FncRegistry& get_caller_registry();

template<typename Func, typename Return, typename... Args>
struct RegisterFnc
{
    RegisterFnc() {
        get_caller_registry().insert(typeid(Func), reinterpret_cast<void*>(call));
    }

    static Return call(const char* data, Args... args) {
        auto& closure = *const_cast<Func*>(reinterpret_cast<const Func*>(data));
        return closure(std::forward<Args>(args)...);
    }

    static RegisterFnc instance;
    static std::pair<int,int> add_to_registry() {
        (void)instance;
        return get_caller_registry().lookup_location(typeid(Func));
    }
};

template<typename Func, typename Return, typename... Args>
RegisterFnc<Func,Return,Args...> RegisterFnc<Func,Return,Args...>::instance;

} //end namespace taskloaf

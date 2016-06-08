#pragma once
#include "tlassert.hpp"

#include <memory>
#include <utility>
#include <cereal/types/utility.hpp>

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

    static Return call(Args... args) {
        auto& closure = *reinterpret_cast<Func*>(this);
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

#pragma once

#include <map>
#include <iostream>

template <typename F>
struct CallFunctorByType
{
    template <typename... Args>
    auto operator()(Args&&... xs) const
    {
        return reinterpret_cast<const F&>(*this)(std::forward<Args>(xs)...);
    }
};

static std::map<std::string,void(*)()> fnc_registry;

template<typename T>
struct RegisteredType
{
    static RegisteredType instance;
    std::string name;
    RegisteredType():
        name(typeid(T).name())
    {
        auto caller = [] () { CallFunctorByType<T> caller; caller(); };
        fnc_registry[name] = caller;
    }
    static std::string do_register() {
        return instance.name; 
    }
};

template<typename T>
RegisteredType<T> RegisteredType<T>::instance;

template <typename F>
auto make_fnc(F f) {
    (void)f;
    return RegisteredType<F>::do_register(); 
}

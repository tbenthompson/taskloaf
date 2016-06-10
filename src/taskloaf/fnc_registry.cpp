#include "fnc_registry.hpp"

#include <map>
#include <vector>
#include <typeindex>

namespace taskloaf {

// use type_info.hash_code as the index for a map pointing to each function.
// unfortunately, it turns out that type_info.hash_code is not perfectly unique,
// so I use a vector to disambiguate and refer to registry entries using a pair
// of integers, with the first referring to the type hash code and the second
// referring to the vector index. 

struct registry_impl {
    std::map<size_t,std::vector<std::pair<std::type_index,void*>>> registry;
};

fnc_registry::fnc_registry():
    impl(std::make_unique<registry_impl>(registry_impl()))
{}

fnc_registry::~fnc_registry() = default;

void fnc_registry::insert(const std::type_info& t_info, void* f_ptr) {
    auto tid = t_info.hash_code();
    impl->registry[tid].push_back({std::type_index(t_info), f_ptr});
}

std::pair<size_t,size_t> 
fnc_registry::lookup_location(const std::type_info& t_info ) {
    auto tid = t_info.hash_code();
    for (size_t i = 0; i < impl->registry[tid].size(); i++) {
        if (std::type_index(t_info) == impl->registry[tid][i].first) {
            return {tid, i};
        }
    }
    TLASSERT(false);
    return {0, 0};
}

void* fnc_registry::get_function(const std::pair<size_t,size_t>& loc) {
    TLASSERT(impl->registry.count(loc.first) > 0);
    TLASSERT(impl->registry[loc.first].size() > loc.second);
    return impl->registry[loc.first][loc.second].second;
}

fnc_registry& get_fnc_registry() {
    static fnc_registry caller_registry;
    return caller_registry;
}

}

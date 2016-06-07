#include "fnc_registry.hpp"

#include <map>
#include <vector>
#include <typeindex>

namespace taskloaf {

struct RegistryImpl {
    std::map<int,std::vector<std::pair<std::type_index,void*>>> registry;
};

FncRegistry::FncRegistry():
    impl(new RegistryImpl())
{}

FncRegistry::~FncRegistry() = default;

void FncRegistry::insert(const std::type_info& t_info, void* f_ptr) {
    auto tid = t_info.hash_code();
    impl->registry[tid].push_back({std::type_index(t_info), f_ptr});
}

std::pair<int,int> 
FncRegistry::lookup_location(const std::type_info& t_info ) {
    auto tid = t_info.hash_code();
    for (size_t i = 0; i < impl->registry[tid].size(); i++) {
        if (std::type_index(t_info) == impl->registry[tid][i].first) {
            return {tid, i};
        }
    }
    tlassert(false);
    return {0, 0};
}

void* FncRegistry::get_function(const std::pair<int,int>& loc) {
    return impl->registry[loc.first][loc.second].second;
}

FncRegistry& get_caller_registry() {
    static FncRegistry caller_registry;
    return caller_registry;
}

}

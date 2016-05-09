#include "fnc.hpp"

#include <map>
#include <vector>
#include <typeindex>

namespace taskloaf {

struct RegistryImpl {
    std::map<size_t,std::vector<std::pair<std::type_index,void*>>> registry;
};

CallerRegistry::CallerRegistry():
    impl(new RegistryImpl())
{}

CallerRegistry::~CallerRegistry() = default;

void CallerRegistry::insert(const std::type_info& t_info, void* f_ptr) {
    auto tid = t_info.hash_code();
    impl->registry[tid].push_back({std::type_index(t_info), f_ptr});
}

std::pair<size_t,size_t> 
CallerRegistry::lookup_location(const std::type_info& t_info ) {
    auto tid = t_info.hash_code();
    for (size_t i = 0; i < impl->registry[tid].size(); i++) {
        if (std::type_index(t_info) == impl->registry[tid][i].first) {
            return {tid, i};
        }
    }
    tlassert(false);
    return {0, 0};
}

void* CallerRegistry::get_function(const std::pair<size_t,size_t>& loc) {
    return impl->registry[loc.first][loc.second].second;
}

CallerRegistry& get_caller_registry() {
    static CallerRegistry caller_registry;
    return caller_registry;
}

}

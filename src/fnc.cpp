#include "fnc.hpp"

namespace taskloaf {

std::map<std::type_index,void(*)(const std::vector<Data*>&)> fnc_registry;

} //end namespace taskloaf

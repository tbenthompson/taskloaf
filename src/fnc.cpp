#include "fnc.hpp"

namespace taskloaf {

std::map<std::string,void(*)(const std::vector<Data*>&)> fnc_registry;

} //end namespace taskloaf

#pragma once

#include <string>

namespace taskloaf {

struct Address {
    //TODO: Consider changing std::string to something else for optimization.
    std::string hostname;
    uint16_t port;
};

} //end namespace taskloaf

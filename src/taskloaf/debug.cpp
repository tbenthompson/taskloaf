#include "debug.hpp"

#include <iostream>
#include <exception>

namespace taskloaf {
void _assert(const char* expression, const char* file, int line)
{
    std::cerr << "Assertion " << expression << " failed, file "
        << file << " line " << line << "." << std::endl;
    std::terminate();
}
}

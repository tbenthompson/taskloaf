#pragma once
#include "closure.hpp"

#include <vector>

namespace taskloaf {
typedef Closure<void()> TaskT;

struct Address;

struct Comm {
    virtual const Address& get_addr() const = 0;
    virtual const std::vector<Address>& remote_endpoints() = 0;
    virtual void send(const Address& dest, TaskT d) = 0;
    virtual TaskT recv() = 0;
};

} //end namespace taskloaf



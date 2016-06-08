#pragma once
#include "closure.hpp"

#include <vector>

namespace taskloaf {

struct Address;

struct Comm {
    virtual const Address& get_addr() const = 0;
    virtual const std::vector<Address>& remote_endpoints() = 0;
    virtual void send(const Address& dest, Closure d) = 0;
    virtual Closure recv() = 0;
};

} //end namespace taskloaf



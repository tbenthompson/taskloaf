#pragma once
#include "closure.hpp"

#include <vector>

namespace taskloaf {

struct address;

struct comm {
    virtual const address& get_addr() const = 0;
    virtual const std::vector<address>& remote_endpoints() = 0;
    virtual void send(const address& dest, closure d) = 0;
    virtual closure recv() = 0;
};

} //end namespace taskloaf



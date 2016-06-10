#pragma once
#include "closure.hpp"

#include <vector>

namespace taskloaf {

struct Address;

struct Comm {
    virtual const Address& get_addr() const = 0;
    virtual const std::vector<Address>& remote_endpoints() = 0;
    virtual void send(const Address& dest, std::unique_ptr<generic_closure> d) = 0;
    virtual std::unique_ptr<generic_closure> recv() = 0;
};

} //end namespace taskloaf



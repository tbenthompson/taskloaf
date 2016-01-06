#pragma once
#include "id.hpp"
#include "address.hpp"

#include <memory>

namespace taskloaf {

struct RingImpl;
struct Comm;
struct Ring {
    std::unique_ptr<RingImpl> impl;

    Ring(Comm& comm, int n_locs);
    Ring(Comm& comm, Address gatekeeper, int n_locs);
    Ring(Ring&&);

    ~Ring();

    void introduce(const Address& their_addr);
    Address get_owner(const ID& id);
    std::vector<Address> ring_members() const;
    size_t ring_size() const;
    const std::vector<ID>& get_locs() const;
};

} //end namespace taskloaf

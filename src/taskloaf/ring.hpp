#pragma once
#include "id.hpp"
#include "address.hpp"

#include <memory>

namespace taskloaf {

typedef std::function<void(ID,ID,Address)> TransferHandler;
struct RingImpl;
struct Comm;
struct Ring {
    std::unique_ptr<RingImpl> impl;

    Ring(Comm& comm, int n_locs);
    Ring(Ring&&);

    ~Ring();

    void introduce(const Address& their_addr);
    void gossip();
    std::vector<std::pair<ID,ID>> compute_transfers(const std::vector<ID>& locs);
    void add_transfer_handler(TransferHandler handler);
    Address get_owner(const ID& id);
    std::vector<Address> ring_members() const;
    size_t ring_size() const;
    const std::vector<ID>& get_locs() const;
};

} //end namespace taskloaf

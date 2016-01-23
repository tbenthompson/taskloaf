#include "ring.hpp"
#include "protocol.hpp"
#include "comm.hpp"
#include "data.hpp"

#include <map>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace taskloaf {

struct RingState {
    Address addr;
    std::vector<ID> locs;
};

struct GossipMessage {
    std::vector<RingState> friend_state;
    RingState sender_state;
    bool respond;
};


template <typename T>
auto loop_around(auto it, const T& collection) {
    if (it == collection.end()) {
        return collection.begin();
    }
    return it;
}

template <typename T>
auto before(auto it, const T& collection) {
    if (it == collection.begin()) {
        return std::prev(collection.end());
    }
    return std::prev(it);
}

struct HashRing {
    std::map<ID,Address> sorted_locs;

    auto succ(const ID& id) const {
        return loop_around(sorted_locs.upper_bound(id), sorted_locs);
    }

    auto pred(const ID& id) const {
        return before(succ(id), sorted_locs);
    }

    void insert(std::vector<ID> locs, Address addr) {
        for (auto l: locs) {
            sorted_locs.insert({l, addr});
        }
    }
};

struct RingImpl {
    Comm& comm;

    std::map<Address,RingState> friends;
    std::vector<ID> my_locs;
    std::vector<TransferHandler> transfer_handlers;
    HashRing hash_ring;

    RingImpl(Comm& comm, int n_locs):
        comm(comm),
        my_locs(new_ids(n_locs))
    {
        hash_ring.insert(my_locs, comm.get_addr());

        comm.add_handler(Protocol::Gossip, [&] (Data d) {
            auto m = d.get_as<GossipMessage>();
            receive_gossip(m);
        });
    } 

    Msg make_gossip_msg(bool request_response) {
        comm.recv();
        GossipMessage msg;
        msg.sender_state = {comm.get_addr(), my_locs};
        for (auto& f: friends) {
            msg.friend_state.push_back(f.second);
        }
        msg.respond = request_response;
        return Msg(Protocol::Gossip, make_data(std::move(msg)));
    }

    void gossip(const Address& their_addr, bool request_response) {
        comm.send(their_addr, make_gossip_msg(request_response));
    }

    void gossip(bool request_response) {
        comm.send_random(make_gossip_msg(request_response));
    }

    std::vector<std::pair<ID,ID>> compute_transfers(const std::vector<ID>& locs) {
        std::vector<std::pair<ID,ID>> transfers;
        for (auto& L: locs) {
            if (hash_ring.pred(L)->second == comm.get_addr()) {
                transfers.push_back({L, hash_ring.succ(L)->first});
            }
        }
        return transfers;
    }

    void add_friend(const RingState& their_state) {
        assert(their_state.addr != comm.get_addr());
        if (friends.count(their_state.addr) > 0) {
            return;
        }
        
        auto transfers = compute_transfers(their_state.locs);
        for (auto& t: transfers) {
            for (auto& h: transfer_handlers) {
                h(t.first, t.second, their_state.addr);
            }
        }
        hash_ring.insert(their_state.locs, their_state.addr);
        friends.insert(std::make_pair(their_state.addr, their_state));
    }

    void receive_gossip(const GossipMessage& msg) {
        for (size_t i = 0; i < msg.friend_state.size(); i++) {
            auto& s = msg.friend_state[i];
            if (s.addr == comm.get_addr()) {
                continue;
            }
            if (friends.count(s.addr) == 0) {
                add_friend(s);
                gossip(s.addr, true);
            }
        }
        add_friend(msg.sender_state);
        if (msg.respond) {
            gossip(msg.sender_state.addr, false);
        }
    }
};

Ring::Ring(Comm& comm, int n_locs):
    impl(std::make_unique<RingImpl>(comm, n_locs))
{}

Ring::Ring(Ring&&) = default;
Ring::~Ring() = default;

void Ring::introduce(const Address& their_addr) {
    if (their_addr == impl->comm.get_addr()) {
        return;
    }
    impl->gossip(their_addr, true);
}

void Ring::gossip() {
    impl->gossip(false);
}

std::vector<std::pair<ID,ID>> Ring::compute_transfers(const std::vector<ID>& locs) {
    return impl->compute_transfers(locs);
}

void Ring::add_transfer_handler(TransferHandler handler) {
    impl->transfer_handlers.push_back(std::move(handler)); 
}

Address Ring::get_owner(const ID& id) {
    return impl->hash_ring.pred(id)->second;
}

std::vector<Address> Ring::ring_members() const {
    std::vector<Address> addrs;
    for (auto& f: impl->friends) {
        addrs.push_back(f.first);
    }
    addrs.push_back(impl->comm.get_addr());
    return addrs;
}

size_t Ring::ring_size() const {
    return impl->friends.size() + 1; 
}

const std::vector<ID>& Ring::get_locs() const {
    return impl->my_locs;
}
} //end namespace taskloaf

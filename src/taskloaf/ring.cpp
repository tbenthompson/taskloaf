#include "ring.hpp"
#include "protocol.hpp"
#include "comm.hpp"
#include "data.hpp"

#include <map>
#include <algorithm>

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

struct ConsistentHash {
    std::map<ID,Address> sorted_locs;

    Address get_owner(const ID& id) {
        auto it = sorted_locs.upper_bound(id);
        if (it == sorted_locs.end()) {
            it = sorted_locs.begin();
        }
        return it->second;
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
    ConsistentHash hash_ring;

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

    void gossip(const Address& their_addr, bool request_response) {
        comm.recv();
        GossipMessage msg;
        msg.sender_state = {comm.get_addr(), my_locs};
        for (auto& f: friends) {
            msg.friend_state.push_back(f.second);
        }
        msg.respond = request_response;

        comm.send(their_addr, Msg(Protocol::Gossip, make_data(std::move(msg))));
    }

    void add_friend(const RingState& their_state) {
        if (friends.count(their_state.addr) > 0) {
            return;
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

Ring::Ring(Comm& comm, Address gatekeeper, int n_locs):
   Ring(comm, n_locs)
{
    introduce(gatekeeper);
}

Ring::Ring(Ring&&) = default;
Ring::~Ring() = default;

void Ring::introduce(const Address& their_addr) {
    impl->gossip(their_addr, true);
}

Address Ring::get_owner(const ID& id) {
    return impl->hash_ring.get_owner(id);
}

size_t Ring::ring_size() const {
    return impl->friends.size() + 1; 
}

const std::vector<ID>& Ring::get_locs() const {
    return impl->my_locs;
}
} //end namespace taskloaf

#pragma once
#include "id.hpp"
#include "address.hpp"

#include <memory>
#include <map>
#include <unordered_map>

namespace caf {
    struct scoped_actor;
    struct actor;
}

namespace taskloaf {

struct GossipState {
    Address addr;
    std::vector<ID> locs;
};

struct GossipMessage {
    std::vector<GossipState> friend_state;
    GossipState sender_state;
    bool respond;
};

struct FriendData {
    GossipState state;
    std::unique_ptr<caf::actor> connection;

    FriendData(FriendData&&) = default;
    FriendData& operator=(FriendData&&) = default;
    FriendData() = default;
    ~FriendData();
};

struct Gossip {
    GossipState my_state;
    std::map<Address,FriendData> friends;
    std::unique_ptr<caf::scoped_actor> comm; 

    Gossip(std::vector<ID> locs);
    ~Gossip();

    void introduce(const Address& their_addr);
    void gossip(const GossipState& their_state);
    void add_friend(const GossipState& their_state);
    void send_message(const caf::actor& connection, bool respond);
    void receive_message(const GossipMessage& msg);
    void handle_messages();
};

template <typename T>
struct Ring {
    std::unordered_map<ID,T> map;

    Gossip gossiper;

    Ring(int n_locs):
        gossiper(new_ids(n_locs))
    {} 

    Ring(Address gatekeeper, int n_locs):
       Ring(n_locs)
    {
        gossiper.introduce(gatekeeper);
    }

    template <typename F>
    void get(const ID& id, F f) {
        f(map[id]);
    }

    void set(const ID& id, T val) {
        map[id] = std::move(val);
    }

    const Address& get_address() const {
        return gossiper.my_state.addr;
    }

    size_t ring_size() const {
        return gossiper.friends.size() + 1; 
    }

    const std::vector<ID>& get_locs() const {
        return gossiper.my_state.locs;
    }

    void handle_messages() {
        gossiper.handle_messages();
    }
};

} //end namespace taskloaf

#pragma once
#include "id.hpp"
#include "address.hpp"
#include "comm.hpp"

#include <memory>
#include <map>
#include <algorithm>
#include <iostream>

namespace taskloaf {

enum class Protocol { Gossip, Set, Get, GetResponse, Erase };

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

    Address get_owner(const ID& id);
    void insert(std::vector<ID> locs, Address addr);
};

struct Ring {
    Comm& comm;

    std::map<Address,RingState> friends;
    std::vector<ID> my_locs;
    ConsistentHash hash_ring;

    std::map<ID,Data> local_data;

    std::map<size_t,std::function<void(Data)>> callbacks;
    size_t next_callback;

    Ring(Comm& comm, int n_locs);
    Ring(Comm& comm, Address gatekeeper, int n_locs);

    void introduce(const Address& their_addr);

    void get(const ID& id, std::function<void(Data)> f);
    void set(const ID& id, Data val);
    void erase(const ID& id);

    template <typename T>
    void set(const ID& id, T val) {
        set(id, make_data(std::move(val)));
    }

    size_t ring_size() const;
    const std::vector<ID>& get_locs() const;


    void gossip(const Address& their_addr, bool request_response);
    void add_friend(const RingState& their_state);
    void receive_gossip(const GossipMessage& msg);
};

} //end namespace taskloaf

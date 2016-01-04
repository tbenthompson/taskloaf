#pragma once
#include "id.hpp"
#include "address.hpp"
#include "comm.hpp"

#include <memory>
#include <map>
#include <algorithm>
#include <iostream>

namespace taskloaf {

enum class Protocol { Gossip, Set, Get, GetResponse };

struct GossipState {
    Address addr;
    //TODO: Maybe change this to Data and then have a consistenthashing struct
    //that controls this data with a add_friend_handler.
    std::vector<ID> locs;
};

struct GossipMessage {
    std::vector<GossipState> friend_state;
    GossipState sender_state;
    bool respond;
};

struct Gossiper {
    GossipState my_state;
    std::map<Address,GossipState> friends;
    std::map<ID,Address> sorted_ids;
    Comm& comm;

    Gossiper(Comm& comm, std::vector<ID> locs);
    ~Gossiper();

    void introduce(const Address& their_addr);
    void gossip(const GossipState& their_state);
    void add_friend(const GossipState& their_state);
    void send_message(const Address& their_addr, bool respond);
    void receive_message(const GossipMessage& msg);
    void handle_messages();
};

template <typename T>
struct Ring {
    std::map<ID,T> map;
    std::map<size_t,std::function<void(T)>> callbacks;
    size_t next_callback;
    Comm& comm;
    Gossiper gossiper;

    Ring(Comm& comm, int n_locs):
        next_callback(0),
        comm(comm),
        gossiper(comm, new_ids(n_locs))
    {
        comm.add_handler(Protocol::Set, [&] (Data d) {
            auto p = d.get_as<std::pair<ID,T>>();
            map[p.first] = p.second;
        });
        comm.add_handler(Protocol::Get, [&] (Data d) {
            auto p = d.get_as<std::tuple<ID,size_t,Address>>();
            auto& val = map[std::get<0>(p)];
            comm.send(std::get<2>(p), Msg(
                Protocol::GetResponse,
                make_data(std::make_pair(std::get<1>(p), val))
            ));
        });
        comm.add_handler(Protocol::GetResponse, [&] (Data d) {
            auto p = d.get_as<std::pair<size_t,T>>();
            callbacks[p.first](p.second);
        });
    } 

    Ring(Comm& comm, Address gatekeeper, int n_locs):
       Ring(comm, n_locs)
    {
        gossiper.introduce(gatekeeper);
    }

    Address get_owner(const ID& id) {
        //TODO: I don't like that the locs are stored in the gossiper, but accessed
        //directly here.
        auto it = gossiper.sorted_ids.upper_bound(id);
        if (it == gossiper.sorted_ids.end()) {
            it = gossiper.sorted_ids.begin();
        }
        return it->second;
    }

    void get(const ID& id, std::function<void(T)> f) {
        auto owner_addr = get_owner(id);
        if (owner_addr == comm.get_addr()) {
            f(map[id]);
        } else {
            auto callback_id = next_callback;
            next_callback++;
            callbacks.insert(std::make_pair(callback_id, std::move(f)));
            comm.send(owner_addr, Msg(
                Protocol::Get,
                make_data(std::make_tuple(id, callback_id, comm.get_addr()))
            ));
        }
    }

    void set(const ID& id, T val) {
        auto owner_addr = get_owner(id);
        if (owner_addr == comm.get_addr()) {
            map[id] = std::move(val);
        } else {
            comm.send(owner_addr, Msg(
                Protocol::Set,
                make_data(std::make_pair(id, std::move(val)))
            ));
        }
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

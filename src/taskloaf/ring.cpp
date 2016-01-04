#include "ring.hpp"

namespace taskloaf {

Address ConsistentHash::get_owner(const ID& id) {
    auto it = sorted_locs.upper_bound(id);
    if (it == sorted_locs.end()) {
        it = sorted_locs.begin();
    }
    return it->second;
}

void ConsistentHash::insert(std::vector<ID> locs, Address addr) {
    for (auto l: locs) {
        sorted_locs.insert({l, addr});
    }
}

Ring::Ring(Comm& comm, int n_locs):
    comm(comm),
    my_locs(new_ids(n_locs)),
    next_callback(0)
{
    hash_ring.insert(my_locs, comm.get_addr());

    comm.add_handler(Protocol::Set, [&] (Data d) {
        auto p = d.get_as<std::pair<ID,Data>>();
        local_data[p.first] = p.second;
    });

    comm.add_handler(Protocol::Get, [&] (Data d) {
        auto p = d.get_as<std::tuple<ID,size_t,Address>>();
        auto& val = local_data[std::get<0>(p)];
        comm.send(std::get<2>(p), Msg(
            Protocol::GetResponse,
            make_data(std::make_pair(std::get<1>(p), val))
        ));
    });

    comm.add_handler(Protocol::GetResponse, [&] (Data d) {
        auto p = d.get_as<std::pair<size_t,Data>>();
        callbacks[p.first](p.second);
    });

    comm.add_handler(Protocol::Erase, [&] (Data d) {
        auto p = d.get_as<ID>();
        local_data.erase(p);
    });

    comm.add_handler(Protocol::Gossip, [&] (Data d) {
        auto m = d.get_as<GossipMessage>();
        receive_gossip(m);
    });
} 

Ring::Ring(Comm& comm, Address gatekeeper, int n_locs):
   Ring(comm, n_locs)
{
    introduce(gatekeeper);
}

void Ring::introduce(const Address& their_addr) {
    gossip(their_addr, true);
}

void Ring::gossip(const Address& their_addr, bool request_response) {
    comm.recv();
    GossipMessage msg;
    msg.sender_state = {comm.get_addr(), my_locs};
    for (auto& f: friends) {
        msg.friend_state.push_back(f.second);
    }
    msg.respond = request_response;

    comm.send(their_addr, Msg(Protocol::Gossip, make_data(std::move(msg))));
}

void Ring::add_friend(const RingState& their_state) {
    if (friends.count(their_state.addr) > 0) {
        return;
    }
    
    hash_ring.insert(their_state.locs, their_state.addr);
    friends.insert(std::make_pair(their_state.addr, their_state));
}

void Ring::receive_gossip(const GossipMessage& msg) {
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

void Ring::get(const ID& id, std::function<void(Data)> f) {
    auto owner_addr = hash_ring.get_owner(id);
    if (owner_addr == comm.get_addr()) {
        f(local_data[id]);
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

void Ring::set(const ID& id, Data val) {
    auto owner_addr = hash_ring.get_owner(id);
    if (owner_addr == comm.get_addr()) {
        local_data[id] = std::move(val);
    } else {
        comm.send(owner_addr, Msg(
            Protocol::Set,
            make_data(std::make_pair(id, std::move(val)))
        ));
    }
}

void Ring::erase(const ID& id) {
    auto owner_addr = hash_ring.get_owner(id);
    if (owner_addr == comm.get_addr()) {
        local_data.erase(id);
    } else {
        comm.send(owner_addr, Msg(Protocol::Erase, make_data(id)));
    }
}

size_t Ring::ring_size() const {
    return friends.size() + 1; 
}

const std::vector<ID>& Ring::get_locs() const {
    return my_locs;
}
} //end namespace taskloaf

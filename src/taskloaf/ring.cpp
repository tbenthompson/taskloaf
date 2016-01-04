#include "ring.hpp"

namespace taskloaf {

Gossiper::Gossiper(Comm& comm, std::vector<ID> locs):
    comm(comm)
{
    my_state = {comm.get_addr(), std::move(locs)};
    for (auto& l: my_state.locs) {
        sorted_ids.insert({l, my_state.addr});
    }
    comm.add_handler(Protocol::Gossip, [&] (Data d) {
        auto m = d.get_as<GossipMessage>();
        receive_message(m);
    });
} 

Gossiper::~Gossiper() {

}

void Gossiper::introduce(const Address& their_addr) {
    handle_messages();
    //Messages must be handled before a connect call or they will be erased.
    send_message(their_addr, true);
}

void Gossiper::gossip(const GossipState& their_state) {
    handle_messages();
    send_message(their_state.addr, false);
}

void Gossiper::add_friend(const GossipState& their_state) {
    if (friends.count(their_state.addr) > 0) {
        return;
    }
    
    for (auto& l: their_state.locs) {
        sorted_ids.insert({l, their_state.addr});
    }
    friends.insert(std::make_pair(their_state.addr, their_state));
}

void Gossiper::send_message(const Address& their_addr, bool respond) {
    GossipMessage msg;
    msg.sender_state = my_state;
    for (auto& f: friends) {
        msg.friend_state.push_back(f.second);
    }
    msg.respond = respond;

    comm.send(their_addr, Msg(Protocol::Gossip, make_data(std::move(msg))));
}

void Gossiper::receive_message(const GossipMessage& msg) {
    for (size_t i = 0; i < msg.friend_state.size(); i++) {
        auto& s = msg.friend_state[i];
        if (s.addr == my_state.addr) {
            continue;
        }
        if (friends.count(s.addr) == 0) {
            add_friend(s);
            gossip(s);
        }
    }
    add_friend(msg.sender_state);
    if (msg.respond) {
        gossip(msg.sender_state);
    }
}

void Gossiper::handle_messages() {
    comm.recv();
}

} //end namespace taskloaf

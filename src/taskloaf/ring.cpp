#include "ring.hpp"
#include "caf_connect.hpp"
#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

using state_atom = caf::atom_constant<caf::atom("meet_atom")>;

FriendData::~FriendData() {}

Gossip::Gossip(std::vector<ID> locs):
    comm(std::make_unique<caf::scoped_actor>())
{
    auto port = caf::io::publish(*comm, 0);
    //TODO: This needs to be changed before a distributed run will work.
    my_state = {{"localhost", port}, std::move(locs)};
} 

Gossip::~Gossip() {

}

void Gossip::introduce(const Address& their_addr) {
    //Messages must be handled before a connect call or they will be erased.
    handle_messages();

    auto connection = connect((*comm).get(), their_addr);
    send_message(connection, true);
}

void Gossip::gossip(const GossipState& their_state) {
    send_message(*friends[their_state.addr].connection, false);
}

void Gossip::add_friend(const GossipState& their_state) {
    if (friends.count(their_state.addr) > 0) {
        return;
    }

    auto connection = connect((*comm).get(), their_state.addr);
    FriendData f{their_state, std::make_unique<caf::actor>(connection)};
    friends.insert(std::make_pair(their_state.addr, std::move(f)));
}

void Gossip::send_message(const caf::actor& connection, bool respond) {
    GossipMessage msg;
    msg.sender_state = my_state;
    for (auto& f: friends) {
        msg.friend_state.push_back(f.second.state);
    }
    msg.respond = respond;

    (*comm)->send(connection, state_atom::value, std::move(msg));
}

void Gossip::receive_message(const GossipMessage& msg) {
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

void Gossip::handle_messages() 
{
    std::vector<GossipMessage> msgs;
    while ((*comm)->has_next_message()) {
        (*comm)->receive(
            [&] (state_atom, GossipMessage m) {
                msgs.push_back(std::move(m));
            },
            [&](caf::ok_atom, caf::node_id&,
                caf::actor_addr& new_connection, std::set<std::string>&) 
            {
                if (new_connection == caf::invalid_actor_addr) {
                    return;
                }

                dest = caf::actor_cast<caf::actor>(new_connection);
                connected = true;
            },
            [&](caf::error_atom, const std::string& errstr) {
                auto wait = 3;
                std::cout << "FAILED CONNECTION " << errstr << std::endl;
                self->delayed_send(
                    mm, std::chrono::seconds(wait), caf::connect_atom::value,
                    to_addr.hostname, to_addr.port
                );
            }
        );
    }
    for (auto& m: msgs) {
        receive_message(m);
    }
}

} //end namespace taskloaf

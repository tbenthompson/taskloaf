#include "comm.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

inline caf::actor connect(caf::blocking_actor* self, Address to_addr)
{
    auto mm = caf::io::get_middleman_actor();
    self->send(mm, caf::connect_atom::value, to_addr.hostname, to_addr.port); 

    caf::actor dest;
    bool connected = false;
    self->do_receive(
        [&](caf::ok_atom, caf::node_id&,
            caf::actor_addr& new_connection, std::set<std::string>&) 
        {
            if (new_connection == caf::invalid_actor_addr) {
                return;
            }

            dest = caf::actor_cast<caf::actor>(new_connection);
            std::cout << "CONNECTED" << std::endl;
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
    ).until([&] {return connected; });

    return dest;
}

struct CAFCommImpl {
    caf::scoped_actor actor;
    Address addr;
    std::map<Address,caf::actor> other_ends;
    std::map<int,std::function<void(Data)>> handlers;
};

CAFComm::CAFComm():
    impl(std::make_unique<CAFCommImpl>())
{
    auto port = caf::io::publish(impl->actor, 0);
    impl->addr = {"localhost", port};
}

CAFComm::~CAFComm() {

}

const Address& CAFComm::get_addr() {
    return impl->addr;
}

void CAFComm::send(const Address& dest, Msg msg) {
    if (impl->other_ends.count(dest) == 0) {
        impl->other_ends[dest] = connect(impl->actor.get(), dest);
    }
    impl->actor->send(impl->other_ends[dest], std::move(msg));
}

void CAFComm::recv() {
    std::vector<Msg> msgs;
    while (impl->actor->has_next_message()) {
        impl->actor->receive(
            [&msgs] (Msg m) {
                msgs.push_back(std::move(m));
            }
        );
    }
    for (auto& m: msgs) {
        impl->handlers[m.msg_type](std::move(m.data));
    }
}

void CAFComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    impl->handlers.insert(std::make_pair(msg_type, std::move(handler)));
}

} //end namespace taskloaf

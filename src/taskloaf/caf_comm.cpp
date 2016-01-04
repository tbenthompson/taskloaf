#include "caf_comm.hpp"
#include "address.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

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
    //TODO: This needs to be changed before a distributed run will work.
    impl->addr = {"localhost", port};
}

CAFComm::~CAFComm() {

}

const Address& CAFComm::get_addr() {
    return impl->addr;
}

void CAFComm::send(const Address& dest, Msg msg) {
    if (impl->other_ends.count(dest) == 0) {
        auto connection = caf::io::remote_actor(dest.hostname, dest.port);
        impl->other_ends[dest] = connection;
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
        if (impl->handlers.count(m.msg_type) == 0) {
            continue;
        }
        impl->handlers[m.msg_type](std::move(m.data));
    }
}

void CAFComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    impl->handlers.insert(std::make_pair(msg_type, std::move(handler)));
}

} //end namespace taskloaf

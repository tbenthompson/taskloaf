#include "caf_comm.hpp"
#include "address.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

struct CAFCommImpl {
    caf::scoped_actor actor;
    Address addr;
    std::map<Address,caf::actor> other_ends;
    std::vector<Address> other_end_addrs;
    std::map<int,std::vector<std::function<void(Data)>>> handlers;
    Msg* cur_msg;

    void insert_other_end(const Address& addr, const caf::actor& actor) {
        other_ends[addr] = actor;
        other_end_addrs.push_back(addr);
    }

    void call_handlers(Msg& m) {
        cur_msg = &m;
        if (handlers.count(m.msg_type) == 0) {
            return;
        }
        for (auto& h: handlers[m.msg_type]) {
            h(m.data);
        }
        cur_msg = nullptr;
    }
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
    if (dest == get_addr()) {
        impl->call_handlers(msg);
        return;
    }
    if (impl->other_ends.count(dest) == 0) {
        //TODO: An asynchronous remote_actor function would be really helpful
        //in having very very fast startup and teardown times for taskloaf.
        auto connection = caf::io::remote_actor(dest.hostname, dest.port);
        impl->insert_other_end(dest, connection);
    }
    impl->actor->send(impl->other_ends[dest], std::move(msg));
}

const std::vector<Address>& CAFComm::remote_endpoints() {
    return impl->other_end_addrs;
}

Msg& CAFComm::cur_message() {
    return *impl->cur_msg;
}

bool CAFComm::has_incoming() {
    return impl->actor->has_next_message();
}

void CAFComm::recv() {
    if (has_incoming()) {
        impl->actor->receive(
            [&] (Msg m) {
                impl->call_handlers(m);
            }
        );
    }
}

void CAFComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    impl->handlers[msg_type].push_back(std::move(handler));
}

} //end namespace taskloaf

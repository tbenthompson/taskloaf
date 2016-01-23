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
    Msg* cur_msg;
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
        //TODO: An asynchronous remote_actor function would be really helpful
        //in having very very fast startup and teardown times for taskloaf.
        auto connection = caf::io::remote_actor(dest.hostname, dest.port);
        impl->other_ends[dest] = connection;
    }
    impl->actor->send(impl->other_ends[dest], std::move(msg));
}

void CAFComm::send_all(Msg msg) {
    for (auto& endpt: impl->other_ends) {
        send(endpt.first, msg); 
    }
}

void CAFComm::send_random(Msg msg) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    if (impl->other_ends.size() == 0) {
        return;
    }
    std::uniform_int_distribution<> dis(0, impl->other_ends.size() - 1);
    auto item = impl->other_ends.begin();
    auto count = dis(gen);
    std::advance(item, count); 
    send(item->first, std::move(msg));
}

void CAFComm::forward(const Address& to) {
    send(to, *impl->cur_msg);
}

bool CAFComm::has_incoming() {
    return impl->actor->has_next_message();
}

void CAFComm::recv() {
    if (has_incoming()) {
        impl->actor->receive(
            [&] (Msg m) {
                impl->cur_msg = &m;
                if (impl->handlers.count(m.msg_type) == 0) {
                    return;
                }
                impl->handlers[m.msg_type](m.data);
                impl->cur_msg = nullptr;
            }
        );
    }
}

void CAFComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    impl->handlers.insert(std::make_pair(msg_type, std::move(handler)));
}

} //end namespace taskloaf

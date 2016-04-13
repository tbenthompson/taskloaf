#include "comm.hpp"
#include "address.hpp"

#include <random>

namespace taskloaf {

Msg::Msg():
    msg_type(0),
    data(empty_data())
{}

Msg::Msg(int msg_type, Data data):
    msg_type(msg_type),
    data(std::move(data))
{}

void MsgHandlers::call(Msg& m) {
    if (handlers.count(m.msg_type) == 0) {
        return;
    }
    for (auto& h: handlers[m.msg_type]) {
        h(m.data);
    }
}

void MsgHandlers::add_handler(int msg_type, std::function<void(Data)> handler) {
    handlers[msg_type].push_back(std::move(handler));
}


void Comm::send_random(Msg msg) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    auto& remotes = remote_endpoints();
    if (remotes.size() == 0) {
        return;
    }
    std::uniform_int_distribution<> dis(0, remotes.size() - 1);
    auto idx = dis(gen);
    send(remotes[idx], std::move(msg));
}

} //end namespace taskloaf

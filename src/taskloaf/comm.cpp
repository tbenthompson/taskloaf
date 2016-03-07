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

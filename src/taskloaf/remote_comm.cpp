#include "remote_comm.hpp"
#include "address.hpp"

#include <iostream>

namespace taskloaf {

RemoteComm::RemoteComm(std::unique_ptr<Messenger> local):
    local_comm(std::move(local))
{}

RemoteComm::~RemoteComm() {

}

// const Address& RemoteComm::get_addr() {
//     return Address{"a", 0};
// }

void RemoteComm::send(const Address& dest, Msg msg) {
    (void)dest;
    (void)msg;
    // local_comm->send(dest, std::move(msg));
}

// const std::vector<Address>& RemoteComm::remote_endpoints() {
//     return {};
//     // return local_comm->remote_endpoints();
// }

bool RemoteComm::has_incoming() {
    return true;
}

void RemoteComm::recv() {

}

// Msg& RemoteComm::cur_message() {
//     // return local_comm->cur_message();
// }

void RemoteComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    (void)msg_type;
    (void)handler;
    // local_comm->add_handler(msg_type, handler);
}

} // end namespace taskloaf

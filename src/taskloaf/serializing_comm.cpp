#include "serializing_comm.hpp"
#include "address.hpp"

// TODO REMOVE:
#include "worker.hpp"
#include "protocol.hpp"
#include <iostream>

namespace taskloaf {

SerializingComm::SerializingComm(std::unique_ptr<Comm> backend):
    backend(std::move(backend))
{}

SerializingComm::~SerializingComm() = default;

const Address& SerializingComm::get_addr() {
    return backend->get_addr();
}

void print_msg_type(std::string prefix, int msg_type) {
    if (msg_type != 2 && msg_type != 3) {
        std::cout << prefix << " on " << cur_worker->core_id << ": " << ProtocolNames[msg_type] << std::endl;
    }
}

void SerializingComm::send(const Address& dest, Msg msg) {
    print_msg_type("Sending ", msg.msg_type);
    std::stringstream serialized_data;
    cereal::BinaryOutputArchive oarchive(serialized_data);
    oarchive(msg.data);
    Msg serialized_msg(msg.msg_type, make_data(serialized_data.str()));
    backend->send(dest, std::move(serialized_msg));
}

const std::vector<Address>& SerializingComm::remote_endpoints() {
    return backend->remote_endpoints();
}

bool SerializingComm::has_incoming() {
    return backend->has_incoming();
}

void SerializingComm::recv() {
    return backend->recv();
}

Msg& SerializingComm::cur_message() {
    return cur_msg;
}

void SerializingComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    backend->add_handler(msg_type, [=] (Data serialized_d) {
        print_msg_type("Received ", msg_type);
        cur_msg.msg_type = msg_type;
        std::stringstream serialized_ss(serialized_d.get_as<std::string>());
        cereal::BinaryInputArchive iarchive(serialized_ss);
        iarchive(cur_msg.data);
        return handler(cur_msg.data);
    });
}

} // end namespace taskloaf

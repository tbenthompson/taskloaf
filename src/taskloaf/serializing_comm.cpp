#include "serializing_comm.hpp"
#include "address.hpp"

namespace taskloaf {

SerializingComm::SerializingComm(std::unique_ptr<Comm> backend):
    backend(std::move(backend))
{}

SerializingComm::~SerializingComm() = default;

const Address& SerializingComm::get_addr() const {
    return backend->get_addr();
}

void SerializingComm::send(const Address& dest, Msg msg) {
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
    return *cur_msg;
}

void SerializingComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    backend->add_handler(msg_type, [=] (Data serialized_d) {
        Msg m;
        cur_msg = &m;
        m.msg_type = msg_type;
        std::stringstream serialized_ss(serialized_d.get_as<std::string>());
        cereal::BinaryInputArchive iarchive(serialized_ss);
        iarchive(m.data);
        handler(m.data);
        cur_msg = nullptr;
    });
}

} // end namespace taskloaf

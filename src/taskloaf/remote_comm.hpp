#pragma once

#include "comm.hpp"

namespace taskloaf {

struct Messenger {

};

// struct RemoteComm: public Comm {
struct RemoteComm {
    std::unique_ptr<Messenger> local_comm;

    RemoteComm(std::unique_ptr<Messenger> local);
    ~RemoteComm();

    // const Address& get_addr();// override;
    void send(const Address& dest, Msg msg);// override;
    // const std::vector<Address>& remote_endpoints();// override;
    bool has_incoming();// override;
    void recv();// override;
    // Msg& cur_message() override;
    void add_handler(int msg_type, std::function<void(Data)> handler);// override; 
};

} //end namespace taskloaf

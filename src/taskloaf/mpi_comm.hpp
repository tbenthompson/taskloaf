#pragma once
#include <mpi.h>
#include "taskloaf/address.hpp"
#include "taskloaf/comm.hpp"

namespace taskloaf {

int mpi_rank(const Comm& c);

struct SentMsg {
    Msg msg;
    MPI_Request state;
};

struct MPIComm: public Comm {
    Address addr;
    std::vector<Address> endpoints;
    MsgHandlers handlers;
    std::vector<SentMsg> outbox;
    Msg* cur_msg;

    MPIComm(); 

    const Address& get_addr() const override;
    void send(const Address& dest, Msg msg) override;
    const std::vector<Address>& remote_endpoints() override;
    bool has_incoming() override;
    void recv() override;
    Msg& cur_message() override;
    void add_handler(int msg_type, std::function<void(Data)> handler) override; 
};

} //end namespace taskloaf

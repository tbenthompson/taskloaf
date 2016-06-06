#pragma once
#ifdef MPI_FOUND
#include "taskloaf/address.hpp"
#include "taskloaf/comm.hpp"

#include <mpi.h>

namespace taskloaf {

int mpi_rank(const Comm& c);

struct SentMsg {
    std::string msg;
    MPI_Request state;
};

struct MPIComm: public Comm {
    Address addr;
    std::vector<Address> endpoints;
    std::vector<SentMsg> outbox;

    MPIComm(); 

    const Address& get_addr() const override;
    const std::vector<Address>& remote_endpoints() override;
    void send(const Address& dest, TaskT d) override;
    TaskT recv() override;
};

} //end namespace taskloaf
#endif

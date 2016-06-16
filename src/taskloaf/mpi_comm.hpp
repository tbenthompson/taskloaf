#pragma once
#ifdef MPI_FOUND
#include "taskloaf/address.hpp"
#include "taskloaf/comm.hpp"

#include <mpi.h>

namespace taskloaf {

int mpi_rank();

struct sent_mpi_msg {
    std::string msg;
    MPI_Request state;
};

struct mpi_comm: public comm {
    address addr;
    std::vector<address> endpoints;
    std::vector<sent_mpi_msg> outbox;

    mpi_comm(); 

    const address& get_addr() const override;
    const std::vector<address>& remote_endpoints() override;
    void send(const address& dest, closure d) override;
    closure recv() override;
};

} //end namespace taskloaf
#endif

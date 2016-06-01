#ifdef MPI_FOUND
#include "mpi_comm.hpp"

#include <algorithm>

namespace taskloaf {

int mpi_rank(const Comm& c) {
    return c.get_addr().id;
}

MPIComm::MPIComm() {
    int rank;
    int size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    addr.id = rank;

    for (int i = 0; i < size; i++) {
        if (i == rank) {
            continue;
        }
        endpoints.push_back({i});
    }
}

const Address& MPIComm::get_addr() const {
    return addr; 
}

void MPIComm::send(const Address& dest, Msg msg) {
    outbox.push_back({std::move(msg), MPI_Request()});  

    auto& str_data = outbox.back().msg.data.get_as<std::string>();
    MPI_Isend(
        &str_data.front(), str_data.size(), MPI_CHAR,
        dest.id, msg.msg_type, MPI_COMM_WORLD, &outbox.back().state
    );

    auto it = std::remove_if(outbox.begin(), outbox.end(), [] (const SentMsg& m) {
        int send_done;
        MPI_Status status;
        MPI_Request_get_status(m.state, &send_done, &status);
        return send_done;
    });
    outbox.erase(it, outbox.end());
}

const std::vector<Address>& MPIComm::remote_endpoints() {
    return endpoints;
}

bool MPIComm::has_incoming() {
    MPI_Status stat;
    int msg_exists;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_exists, &stat);
    return msg_exists;
}

void MPIComm::recv() {
    MPI_Status stat;
    int msg_exists;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_exists, &stat);
    if (!msg_exists) {
        return;
    }

    int n_bytes;
    MPI_Get_count(&stat, MPI_CHAR, &n_bytes);

    Msg m(stat.MPI_TAG, make_data(std::string(n_bytes, 'a')));
    cur_msg = &m;
    MPI_Recv(
        &m.data.get_as<std::string>().front(), n_bytes, MPI_CHAR,
        MPI_ANY_SOURCE, MPI_ANY_TAG,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
    handlers.call(m);
    cur_msg = nullptr;
}

Msg& MPIComm::cur_message() {
    return *cur_msg; 
}

void MPIComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    handlers.add_handler(msg_type, std::move(handler));
}

} //end namespace taskloaf
#endif

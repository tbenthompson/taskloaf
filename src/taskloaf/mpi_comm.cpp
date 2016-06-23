#ifdef MPI_FOUND
#include "mpi_comm.hpp"

#include <algorithm>

namespace taskloaf {

thread_local int mpi_comm::next_tag = 1 << 30;

int mpi_rank() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank;
}

mpi_comm::mpi_comm() {
    tag = next_tag;
    next_tag++;

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

const address& mpi_comm::get_addr() const {
    return addr; 
}

void mpi_comm::send(const address& dest, closure msg) {
    std::stringstream serialized_data;
    cereal::BinaryOutputArchive oarchive(serialized_data);
    oarchive(msg);

    outbox.push_back({serialized_data.str(), MPI_Request()});  

    auto& str_data = outbox.back().msg;
    MPI_Isend(
        &str_data.front(), str_data.size(), MPI_CHAR,
        dest.id, tag, MPI_COMM_WORLD, &outbox.back().state
    );

    auto it = std::remove_if(outbox.begin(), outbox.end(), [] (const sent_mpi_msg& m) {
        int send_done;
        MPI_Status status;
        MPI_Request_get_status(m.state, &send_done, &status);
        return send_done;
    });
    outbox.erase(it, outbox.end());
}

const std::vector<address>& mpi_comm::remote_endpoints() {
    return endpoints;
}

closure mpi_comm::recv() {
    MPI_Status stat;
    int msg_exists;
    MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &msg_exists, &stat);
    if (!msg_exists) {
        return closure();
    }

    int n_bytes;
    MPI_Get_count(&stat, MPI_CHAR, &n_bytes);

    std::string s(n_bytes, 'a');
    MPI_Recv(
        &s.front(),
        n_bytes, MPI_CHAR,
        MPI_ANY_SOURCE, tag,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );

    std::stringstream serialized_ss(s);
    cereal::BinaryInputArchive iarchive(serialized_ss);
    closure t;
    iarchive(t);

    return t;
}

} //end namespace taskloaf
#endif

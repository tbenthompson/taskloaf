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

mpi_comm::~mpi_comm() {
    while (outbox.size() > 0) {
        std::cout << "CLEANUP!" << std::endl;
        cleanup();
    }
}

const address& mpi_comm::get_addr() const {
    return addr; 
}

void mpi_comm::cleanup() {
    auto it = std::remove_if(outbox.begin(), outbox.end(), [] (const sent_mpi_msg& m) {
        int send_done;
        MPI_Request_get_status(m.state, &send_done, MPI_STATUS_IGNORE);
        return send_done;
    });
    outbox.erase(it, outbox.end());
}

void mpi_comm::send(const address& dest, closure msg) {
    cleanup();

    std::stringstream serialized_data;
    cereal::BinaryOutputArchive oarchive(serialized_data);
    oarchive(msg);

    outbox.push_back({serialized_data.str(), MPI_Request()});  

    auto& str_data = outbox.back().msg;
    // std::cout << addr.id << " sending " << str_data.size() << " to " << dest.id << std::endl;
    MPI_Isend(
        &str_data.front(), str_data.size(), MPI_CHAR,
        dest.id, tag, MPI_COMM_WORLD, &outbox.back().state
    );
}

const std::vector<address>& mpi_comm::remote_endpoints() {
    return endpoints;
}

closure mpi_comm::recv() {
    cleanup();

    MPI_Status stat;
    int msg_exists;
    MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &msg_exists, &stat);
    if (!msg_exists) {
        return closure();
    }

    int n_bytes;
    MPI_Get_count(&stat, MPI_CHAR, &n_bytes);

    // std::cout << addr.id << " receiving " << n_bytes << " from " << stat.MPI_SOURCE << std::endl;
    // TODO: Reuse the buffer?
    std::string s(n_bytes, 'a');
    // It's important here to only receive a message from the MPI_SOURCE that
    // Iprobe found. If we allow any source, then the message received may be 
    // from a different source and will have a different count, resulting in
    // truncated messages.
    MPI_Recv(
        &s.front(),
        n_bytes, MPI_CHAR,
        stat.MPI_SOURCE, tag, 
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

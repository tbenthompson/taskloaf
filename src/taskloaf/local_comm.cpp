#include "local_comm.hpp"

namespace taskloaf {

local_comm_queues::local_comm_queues(size_t n_workers)
{
    for (size_t i = 0; i < n_workers; i++) {
        qs.emplace_back(starting_queue_size);
    }
}
    
local_comm::local_comm(local_comm_queues& qs, uint16_t my_index):
    queues(qs),
    my_addr{my_index}
{
    for (size_t i = 0; i < queues.qs.size(); i++) {
        if (i == my_index) {
            continue;
        }
        remotes.push_back({static_cast<uint16_t>(i)});
    }
}

const address& local_comm::get_addr() const {
    return my_addr;
}

const std::vector<address>& local_comm::remote_endpoints() {
    return remotes;
}

void local_comm::send(const address& dest, closure msg) {
    queues.qs[dest.id].enqueue(std::move(msg));
}

closure local_comm::recv() {
    closure cur_msg;
    queues.qs[my_addr.id].try_dequeue(cur_msg);
    return cur_msg;
}

} //end namespace taskloaf;

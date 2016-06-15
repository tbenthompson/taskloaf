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
    cleanup();
    outbox.push({std::move(msg), false});
    queues.qs[dest.id].enqueue(&outbox.back());
}

closure local_comm::recv() {
    cleanup();
    msg* cur_msg = nullptr;
    queues.qs[my_addr.id].try_dequeue(cur_msg);
    if (cur_msg == nullptr) {
        return closure();
    }
    return closure(
        [] (msg* m, data& val) {
            m->done = true;
            return m->c(val);
        },
        cur_msg
    );
}

void local_comm::cleanup() {
    if (outbox.size() > 0 && outbox.front().done) {
        outbox.pop();
        cleanup();
    }
}

} //end namespace taskloaf;

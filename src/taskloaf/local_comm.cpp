#include "local_comm.hpp"

namespace taskloaf {

LocalCommQueues::LocalCommQueues(size_t n_workers)
{
    for (size_t i = 0; i < n_workers; i++) {
        qs.emplace_back(starting_queue_size);
    }
}
    
LocalComm::LocalComm(std::shared_ptr<LocalCommQueues> qs, uint16_t my_index):
    queues(qs),
    my_addr{my_index}
{
    for (size_t i = 0; i < queues->qs.size(); i++) {
        if (i == my_index) {
            continue;
        }
        remotes.push_back({static_cast<uint16_t>(i)});
    }
}

const Address& LocalComm::get_addr() const {
    return my_addr;
}

const std::vector<Address>& LocalComm::remote_endpoints() {
    return remotes;
}

void LocalComm::send(const Address& dest, TaskT msg) {
    queues->qs[dest.id].enqueue(std::move(msg));
}

TaskT LocalComm::recv() {
    TaskT cur_msg;
    queues->qs[my_addr.id].try_dequeue(cur_msg);
    return cur_msg;
}

} //end namespace taskloaf;

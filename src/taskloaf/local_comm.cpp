#include "local_comm.hpp"

namespace taskloaf {

LocalCommQueues::LocalCommQueues(size_t n_workers)
{
    for (size_t i = 0; i < n_workers; i++) {
        msg_queues.emplace_back(starting_queue_size);
        cur_msg.emplace_back(std::make_pair(false, Msg()));
    }
}

size_t LocalCommQueues::n_workers() {
    return msg_queues.size();
}

void LocalCommQueues::enqueue(size_t which, Msg msg) {
    tlassert(which < msg_queues.size());
    msg_queues[which].enqueue(std::move(msg));
}

bool LocalCommQueues::has_incoming(size_t which) {
    tlassert(which < msg_queues.size());
    bool& ready_msg = cur_msg[which].first;
    if (!ready_msg) {
        ready_msg = msg_queues[which].try_dequeue(cur_msg[which].second);
    }
    return ready_msg;
}

Msg& LocalCommQueues::front(size_t which) {
    tlassert(which < msg_queues.size());
    return cur_msg[which].second;
}

void LocalCommQueues::pop_front(size_t which) {
    tlassert(which < msg_queues.size());
    cur_msg[which].first = false;
}
    
LocalComm::LocalComm(std::shared_ptr<LocalCommQueues> qs, uint16_t my_index):
    queues(qs),
    my_addr{my_index}
{
    for (size_t i = 0; i < queues->n_workers(); i++) {
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

void LocalComm::send(const Address& dest, Msg msg) {
    queues->enqueue(dest.id, std::move(msg));
}

Msg& LocalComm::cur_message() {
    return *cur_msg;
}

bool LocalComm::has_incoming() {
    return queues->has_incoming(my_addr.id);
}

void LocalComm::recv() {
    if (has_incoming()) {
        auto m = std::move(queues->front(my_addr.id));
        queues->pop_front(my_addr.id);
        cur_msg = &m;
        handlers.call(m);
        cur_msg = nullptr;
    }
}

void LocalComm::add_handler(int msg_type, std::function<void(Data)> handler) {
    handlers.add_handler(msg_type, std::move(handler));
}

} //end namespace taskloaf;

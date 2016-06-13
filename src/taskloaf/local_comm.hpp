#pragma once
#include "address.hpp"
#include "comm.hpp"

#include "concurrentqueue.h"

#include <map>


namespace taskloaf {

using MsgQueue = moodycamel::ConcurrentQueue<closure>;

struct local_comm_queues {
    const size_t starting_queue_size = 20;
    std::vector<MsgQueue> qs;

    local_comm_queues(size_t n_workers);
};

struct local_comm: public comm {
    std::shared_ptr<local_comm_queues> queues;
    std::vector<address> remotes;
    address my_addr;
    bool ready_msg = false;
    closure cur_msg;

    local_comm(std::shared_ptr<local_comm_queues> qs, uint16_t my_index);

    const address& get_addr() const override;
    const std::vector<address>& remote_endpoints() override;
    void send(const address& dest, closure d) override;
    closure recv() override;
};

} //end namespace taskloaf

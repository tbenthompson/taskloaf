#pragma once
#include "address.hpp"
#include "comm.hpp"

#include "concurrentqueue.h"

#include <map>


namespace taskloaf {

using MsgQueue = moodycamel::ConcurrentQueue<Closure>;

struct LocalCommQueues {
    const size_t starting_queue_size = 20;
    std::vector<MsgQueue> qs;

    LocalCommQueues(size_t n_workers);
};

struct LocalComm: public Comm {
    std::shared_ptr<LocalCommQueues> queues;
    std::vector<Address> remotes;
    Address my_addr;
    bool ready_msg = false;
    Closure cur_msg;

    LocalComm(std::shared_ptr<LocalCommQueues> qs, uint16_t my_index);

    const Address& get_addr() const override;
    const std::vector<Address>& remote_endpoints() override;
    void send(const Address& dest, Closure d) override;
    Closure recv() override;
};

} //end namespace taskloaf

#pragma once
#include "address.hpp"
#include "comm.hpp"

#include "concurrentqueue.h"

#include <map>


namespace taskloaf {

using MsgQueue = moodycamel::ConcurrentQueue<Msg>;

//TODO: These two classes can be pimpled.
struct LocalCommQueues {
    const size_t starting_queue_size = 20;
    std::vector<MsgQueue> msg_queues;
    std::vector<std::pair<bool,Msg>> cur_msg;

    LocalCommQueues(size_t n_workers);
    size_t n_workers();
    void enqueue(size_t which, Msg msg);
    bool has_incoming(size_t which);
    Msg& front(size_t which); 
    void pop_front(size_t which);
};

struct LocalComm: public Comm {
    std::shared_ptr<LocalCommQueues> queues;
    std::vector<Address> remotes;
    Address my_addr;
    std::map<int,std::vector<std::function<void(Data)>>> handlers;
    Msg* cur_msg;

    LocalComm(std::shared_ptr<LocalCommQueues> qs, uint16_t my_index);

    void call_handlers(Msg& m);
    const Address& get_addr() override;
    const std::vector<Address>& remote_endpoints() override;
    void send(const Address& dest, Msg msg) override;
    Msg& cur_message() override;
    bool has_incoming() override;
    void recv() override;
    void add_handler(int msg_type, std::function<void(Data)> handler) override;
};

} //end namespace taskloaf

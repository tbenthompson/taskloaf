#pragma once
#include "address.hpp"
#include "comm.hpp"

#include "concurrentqueue.h"

#include <map>
//TODO: REMOVE 
#include <iostream>


namespace taskloaf {

using MsgQueue = moodycamel::ConcurrentQueue<Msg>;

struct LocalCommQueues {
    const size_t starting_queue_size = 20;
    std::vector<MsgQueue> msg_queues;
    std::vector<std::pair<bool,Msg>> cur_msg;

    LocalCommQueues(size_t n_workers)
    {
        for (size_t i = 0; i < n_workers; i++) {
            msg_queues.emplace_back(starting_queue_size);
            cur_msg.emplace_back(std::make_pair(false, Msg()));
        }
    }

    size_t n_workers() {
        return msg_queues.size();
    }

    void enqueue(size_t which, Msg msg) {
        assert(which < msg_queues.size());
        msg_queues[which].enqueue(std::move(msg));
    }

    bool has_incoming(size_t which) {
        assert(which < msg_queues.size());
        bool& ready_msg = cur_msg[which].first;
        if (!ready_msg) {
            ready_msg = msg_queues[which].try_dequeue(cur_msg[which].second);
        }
        return ready_msg;
    }

    Msg& front(size_t which) {
        assert(which < msg_queues.size());
        return cur_msg[which].second;
    }

    void pop_front(size_t which) {
        assert(which < msg_queues.size());
        cur_msg[which].first = false;
    }
};

struct LocalComm: public Comm {
    std::shared_ptr<LocalCommQueues> queues;
    std::vector<Address> remotes;
    Address my_addr;
    std::map<int,std::vector<std::function<void(Data)>>> handlers;
    Msg* cur_msg;

    LocalComm(std::shared_ptr<LocalCommQueues> qs, uint16_t my_index):
        queues(qs),
        my_addr{"", my_index}
    {
        for (size_t i = 0; i < queues->n_workers(); i++) {
            if (i == my_index) {
                continue;
            }
            remotes.push_back({"", static_cast<uint16_t>(i)});
        }
    }

    void call_handlers(Msg& m) {
        if (handlers.count(m.msg_type) == 0) {
            return;
        }
        for (auto& h: handlers[m.msg_type]) {
            h(m.data);
        }
    }

    const Address& get_addr() override {
        return my_addr;
    }

    const std::vector<Address>& remote_endpoints() override {
        return remotes;
    }

    void send(const Address& dest, Msg msg) override {
        queues->enqueue(dest.port, std::move(msg));
    }

    Msg& cur_message() override {
        return *cur_msg;
    }

    bool has_incoming() override {
        return queues->has_incoming(my_addr.port);
    }

    void recv() override {
        if (has_incoming()) {
            auto m = std::move(queues->front(my_addr.port));
            queues->pop_front(my_addr.port);
            cur_msg = &m;
            call_handlers(m);
            cur_msg = nullptr;
        }
    }

    void add_handler(int msg_type, std::function<void(Data)> handler) override {
        handlers[msg_type].push_back(std::move(handler));
    }
};

} //end namespace taskloaf

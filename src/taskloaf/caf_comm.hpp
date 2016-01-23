#pragma once
#include "comm.hpp"

#include <memory>

namespace taskloaf {

struct CAFCommImpl;
struct CAFComm: public Comm {
    std::unique_ptr<CAFCommImpl> impl;

    CAFComm();
    ~CAFComm();

    const Address& get_addr() override;
    void send(const Address& dest, Msg msg) override;
    //TODO: Better architecture for these send_ functions?
    void send_all(Msg msg) override;
    void send_random(Msg msg) override;
    void forward(const Address& to) override;
    bool has_incoming() override;
    void recv() override;
    void add_handler(int msg_type, std::function<void(Data)> handler) override;
};

} //end namespace taskloaf

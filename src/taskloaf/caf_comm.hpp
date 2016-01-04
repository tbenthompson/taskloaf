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
    void recv() override;
    void add_handler(int msg_type, std::function<void(Data)> handler) override;
};

} //end namespace taskloaf

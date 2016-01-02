#pragma once

#include <functional>
#include <memory>
#include "address.hpp"
#include "data.hpp"

namespace taskloaf {

struct Msg {
    int msg_type; 
    Data data;
};

struct Comm {
    virtual const Address& get_addr() = 0;
    virtual void send(const Address& dest, Msg msg) = 0;
    virtual void recv() = 0;
    virtual void add_handler(int msg_type, std::function<void(Data)> handler) = 0; 
};

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

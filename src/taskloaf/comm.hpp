#pragma once
#include "data.hpp"

#include <functional>

namespace taskloaf {

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

struct Msg {
    int msg_type; 
    Data data;

    Msg(int msg_type, Data data):
        msg_type(msg_type),
        data(std::move(data))
    {}

    template <typename T>
    Msg(T msg_type, Data data):
        msg_type(to_underlying(msg_type)),
        data(std::move(data))
    {}
};

struct Address;

struct Comm {
    virtual const Address& get_addr() = 0;
    virtual void send(const Address& dest, Msg msg) = 0;
    virtual void send_all(Msg msg) = 0;
    virtual void send_random(Msg msg) = 0;
    virtual bool has_incoming() = 0;
    virtual void recv() = 0;
    virtual void forward(const Address& to) = 0;
    virtual void add_handler(int msg_type, std::function<void(Data)> handler) = 0; 

    template <typename T>
    void add_handler(T msg_type, std::function<void(Data)> handler) {
        add_handler(to_underlying(msg_type), std::move(handler));
    }
};

} //end namespace taskloaf

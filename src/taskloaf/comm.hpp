#pragma once
#include "data.hpp"

#include <map>
#include <functional>
#include <vector>

namespace taskloaf {

template <typename E>
constexpr auto to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

struct Msg {
    int msg_type; 
    Data data;

    Msg();
    Msg(int msg_type, Data data);

    template <typename T>
    Msg(T msg_type, Data data):
        msg_type(to_underlying(msg_type)),
        data(std::move(data))
    {}
};

struct MsgHandlers {
    std::map<int,std::vector<std::function<void(Data)>>> handlers;

    void call(Msg& m);
    void add_handler(int msg_type, std::function<void(Data)> handler);
};

struct Address;

struct Comm {
    virtual const Address& get_addr() const = 0;
    virtual void send(const Address& dest, Msg msg) = 0;
    virtual const std::vector<Address>& remote_endpoints() = 0;
    virtual bool has_incoming() = 0;
    virtual void recv() = 0;
    virtual Msg& cur_message() = 0;
    virtual void add_handler(int msg_type, std::function<void(Data)> handler) = 0; 

    template <typename T>
    void add_handler(T msg_type, std::function<void(Data)> handler) {
        add_handler(to_underlying(msg_type), std::move(handler));
    }

    void send_random(Msg msg);
};

} //end namespace taskloaf



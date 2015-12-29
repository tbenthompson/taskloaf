#pragma once

#include <string>

namespace taskloaf {

struct Address {
    std::string hostname;
    uint16_t port;
};

inline bool operator<(const Address& a, const Address& b) {
    if (a.hostname == b.hostname) {
        return a.port < b.port;
    }
    return a.hostname < b.hostname;
}

inline bool operator==(const Address& a, const Address& b) {
    return a.port == b.port && a.hostname == b.hostname;
}

inline bool operator!=(const Address& a, const Address& b) {
    return !(a == b);
}

} //end namespace taskloaf

#pragma once

#include <string>

namespace taskloaf {

struct Address {
    //TODO: Consider changing std::string to something else for optimization.
    //Maybe some kind of hashing that's consistent across nodes? Give each 
    //node a random ID?
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
    return a.hostname == b.hostname && a.port == b.port;
}

inline bool operator!=(const Address& a, const Address& b) {
    return !(a == b);
}

} //end namespace taskloaf

#include "address.hpp"

#include <iostream>

namespace taskloaf {

std::ostream& operator<<(std::ostream& os, const Address& a) {
    os << a.hostname << ":" << a.port;
    return os;
}

bool operator<(const Address& a, const Address& b) {
    if (a.hostname == b.hostname) {
        return a.port < b.port;
    }
    return a.hostname < b.hostname;
}

bool operator==(const Address& a, const Address& b) {
    return a.port == b.port && a.hostname == b.hostname;
}

bool operator!=(const Address& a, const Address& b) {
    return !(a == b);
}

} //end namespace taskloaf 

#include "address.hpp"

#include <iostream>

namespace taskloaf {

std::ostream& operator<<(std::ostream& os, const Address& a) {
    os << a.id;
    return os;
}

bool operator<(const Address& a, const Address& b) {
    return a.id < b.id;
}

bool operator==(const Address& a, const Address& b) {
    return a.id == b.id;
}

bool operator!=(const Address& a, const Address& b) {
    return !(a == b);
}

} //end namespace taskloaf 

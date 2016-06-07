#include "address.hpp"

#include <iostream>

namespace taskloaf {

void Address::save(cereal::BinaryOutputArchive& ar) const { ar(id); }

void Address::load(cereal::BinaryInputArchive& ar) { ar(id); }

bool operator<(const Address& a, const Address& b) { return a.id < b.id; }
bool operator==(const Address& a, const Address& b) { return a.id == b.id; }
bool operator!=(const Address& a, const Address& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, const Address& a) {
    os << a.id;
    return os;
}

} //end namespace taskloaf 

#include "address.hpp"

#include <iostream>

namespace taskloaf {

void address::save(cereal::BinaryOutputArchive& ar) const { ar(id); }
void address::load(cereal::BinaryInputArchive& ar) { ar(id); }

bool operator<(const address& a, const address& b) { return a.id < b.id; }
bool operator==(const address& a, const address& b) { return a.id == b.id; }
bool operator!=(const address& a, const address& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, const address& a) {
    os << a.id;
    return os;
}

} //end namespace taskloaf 

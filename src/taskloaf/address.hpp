#pragma once

#include <iosfwd>

#include <cereal/archives/binary.hpp>

namespace taskloaf {

//TODO: remote id and local id?
struct Address {
    int id;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

bool operator<(const Address& a, const Address& b);
bool operator==(const Address& a, const Address& b);
bool operator!=(const Address& a, const Address& b);

std::ostream& operator<<(std::ostream& os, const Address& a);

} //end namespace taskloaf

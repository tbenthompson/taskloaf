#pragma once

#include <iosfwd>

#include <cereal/archives/binary.hpp>

namespace taskloaf {

//TODO: remote id and local id?
struct address {
    int id;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);
};

bool operator<(const address& a, const address& b);
bool operator==(const address& a, const address& b);
bool operator!=(const address& a, const address& b);

std::ostream& operator<<(std::ostream& os, const address& a);

} //end namespace taskloaf

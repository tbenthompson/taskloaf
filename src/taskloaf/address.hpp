#pragma once

#include <iosfwd>

#include <cereal/archives/binary.hpp>

namespace taskloaf {

enum location {
    anywhere = -2,
    here = -1
};

//TODO: remote id and local id?
struct address {
    int id;

    address() = default;
    address(int id): id(id) {}

    void save(cereal::BinaryOutputArchive& ar) const { ar(id); }
    void load(cereal::BinaryInputArchive& ar) { ar(id); }
    bool operator<(const address& b) const { return id < b.id; }
    bool operator==(const address& b) const { return id == b.id; }
    bool operator!=(const address& b) const { return !(*this == b); }
};

std::ostream& operator<<(std::ostream& os, const address& a);

inline std::ostream& operator<<(std::ostream& os, const address& a) {
    os << a.id;
    return os;
}

} //end namespace taskloaf

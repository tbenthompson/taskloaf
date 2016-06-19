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

inline void address::save(cereal::BinaryOutputArchive& ar) const { ar(id); }
inline void address::load(cereal::BinaryInputArchive& ar) { ar(id); }

inline bool operator<(const address& a, const address& b) { return a.id < b.id; }
inline bool operator==(const address& a, const address& b) { return a.id == b.id; }
inline bool operator!=(const address& a, const address& b) { return !(a == b); }

inline std::ostream& operator<<(std::ostream& os, const address& a) {
    os << a.id;
    return os;
}

} //end namespace taskloaf

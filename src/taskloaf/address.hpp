#pragma once

#include <string>
#include <iosfwd>

namespace taskloaf {

// TODO: Probably should rename now that this doesn't
// fit hostname:port anymore
struct Address {
    int id;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(id);
    }
};

std::ostream& operator<<(std::ostream& os, const Address& a);
bool operator<(const Address& a, const Address& b);
bool operator==(const Address& a, const Address& b);
bool operator!=(const Address& a, const Address& b);

} //end namespace taskloaf

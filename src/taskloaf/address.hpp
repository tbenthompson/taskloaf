#pragma once

#include <string>
#include <iosfwd>

namespace taskloaf {

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

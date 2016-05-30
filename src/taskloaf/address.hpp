#pragma once

#include <string>
#include <iosfwd>

namespace taskloaf {

struct Address {
    std::string hostname;
    uint16_t port;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(hostname);
        ar(port);
    }
};

std::ostream& operator<<(std::ostream& os, const Address& a);
bool operator<(const Address& a, const Address& b);
bool operator==(const Address& a, const Address& b);
bool operator!=(const Address& a, const Address& b);

} //end namespace taskloaf

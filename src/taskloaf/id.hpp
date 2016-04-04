#pragma once
#include <vector>
#include <ostream>

namespace taskloaf {

struct ID
{
    size_t firsthalf;
    size_t secondhalf;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(firsthalf);
        ar(secondhalf);
    }
};

bool operator==(const ID& lhs, const ID& rhs);
bool operator!=(const ID& lhs, const ID& rhs);
bool operator<(const ID& lhs, const ID& rhs);
bool operator<=(const ID& lhs, const ID& rhs);

std::ostream& operator<<(std::ostream& os, const ID& id);

size_t random_sizet();
ID new_id();
std::vector<ID> new_ids(size_t count);

} //end namespace taskloaf

/* Define a hash function for IDs so that they can be used in hash tables.
 */
namespace std {
    template <>
    struct hash<taskloaf::ID> {
        size_t operator()(const taskloaf::ID& id) const;
    };
} // end namespace std

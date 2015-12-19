#pragma once
#include <vector>
#include <ostream>

namespace taskloaf {

/* A very simple 128-bit number -- A UUID/GUID
 */
struct ID
{
    size_t firsthalf;
    size_t secondhalf;
};

bool operator==(const ID& lhs, const ID& rhs);
bool operator!=(const ID& lhs, const ID& rhs);
bool operator<(const ID& lhs, const ID& rhs);

std::ostream& operator<<(std::ostream& os, const ID& id);

/* Create a new task or data ID. */
ID new_id();

/* Create multiple new task or data IDs. */
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

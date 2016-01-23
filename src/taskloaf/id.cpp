#include "taskloaf/id.hpp"

#include <random>

namespace taskloaf {

bool operator==(const ID& lhs, const ID& rhs)
{
    return (lhs.firsthalf == rhs.firsthalf) && (lhs.secondhalf == rhs.secondhalf);
}

bool operator!=(const ID& lhs, const ID& rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ID& lhs, const ID& rhs)
{
    if (lhs.firsthalf == rhs.firsthalf) {
        return lhs.secondhalf < rhs.secondhalf;
    }
    return lhs.firsthalf < rhs.firsthalf;
}

bool operator<=(const ID& lhs, const ID& rhs) {
    return lhs < rhs || lhs == rhs;
}

std::ostream& operator<<(std::ostream& os, const ID& id)
{
    os << id.firsthalf << "-" << id.secondhalf;
    return os;
}

size_t random_sizet() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());

    std::uniform_int_distribution<size_t> distribution(
        0, std::numeric_limits<size_t>::max()
    );
    return distribution(gen);
}

ID new_id()
{
    return {random_sizet(), random_sizet()};
}

std::vector<ID> new_ids(size_t count)
{
    std::vector<ID> out(count);
    for (size_t i = 0; i < count; i++) {
        out[i] = new_id();
    }
    return out;
}

} // end namespace taskloaf


namespace std {

size_t hash<taskloaf::ID>::operator()(const taskloaf::ID& id) const
{
    return id.firsthalf;
}

} // end namespace std

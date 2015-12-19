#include "id.hpp"

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

std::ostream& operator<<(std::ostream& os, const ID& id)
{
    os << id.firsthalf << "-" << id.secondhalf;
    return os;
}

ID new_id()
{
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());

    std::uniform_int_distribution<size_t> distribution(
        0, std::numeric_limits<size_t>::max()
    );

    return {distribution(gen), distribution(gen)};
}

} // end namespace taskloaf


namespace std {

size_t hash<taskloaf::ID>::operator()(const taskloaf::ID& id) const
{
    return id.firsthalf;
}

} // end namespace std

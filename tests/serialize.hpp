#pragma once
#include "taskloaf.hpp"

namespace taskloaf {

template <typename T>
std::string serialize(T&& t) {
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(std::forward<T>(t));
    return ss.str();
}

template <typename T>
T deserialize(std::string s) {
    std::stringstream ss(s);
    cereal::BinaryInputArchive iarchive(ss);
    T d2;
    iarchive(d2);
    return d2;
}

} // end namespace taskloaf

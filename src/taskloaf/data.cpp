#include "data.hpp"

namespace taskloaf {

void Data::save(cereal::BinaryOutputArchive& ar) const {
    tlassert(ptr != nullptr);
    ar(deserializer);
    serializer(*this, ar);
}

void Data::load(cereal::BinaryInputArchive& ar) {
    ar(deserializer);
    deserializer(*this, ar);
}

Data empty_data() {
    return Data();
}

} // end namespace taskloaf 

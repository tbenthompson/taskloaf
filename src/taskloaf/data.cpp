#include "data.hpp"

namespace taskloaf {

void Data::save(cereal::BinaryOutputArchive& ar) const {
    tlassert(ptr != nullptr);
    my_serializer(*this, ar);
}

void Data::load(cereal::BinaryInputArchive& ar) {
    Function<void(Data&,cereal::BinaryInputArchive)> deserializer;
    ar(deserializer);
    deserializer(*this, ar);
}

Data empty_data() {
    return Data();
}

} // end namespace taskloaf 

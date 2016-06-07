#include "data.hpp"

namespace taskloaf {

void Data::save(cereal::BinaryOutputArchive& ar) const {
    tlassert(ptr != nullptr);
    serializer(*this, ar);
}

void Data::load(cereal::BinaryInputArchive& ar) {
    std::pair<int,int> deserializer_id;
    ar(deserializer_id);
    auto deserializer = reinterpret_cast<DeserializerT>(
        get_caller_registry().get_function(deserializer_id)
    );
    const char* x = "";
    deserializer(x, *this, ar);
}

bool Data::operator==(std::nullptr_t) const { return ptr == nullptr; }

ConvertibleData Data::convertible() {
    return {*this};
}

} // end namespace taskloaf 

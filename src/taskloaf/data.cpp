#include "data.hpp"

namespace taskloaf {

void Data::save(cereal::BinaryOutputArchive& ar) const {
    bool is_null = (*this == nullptr);
    ar(is_null);
    if (!is_null) {
        serializer(*this, ar);
    }
}

void Data::load(cereal::BinaryInputArchive& ar) {
    bool is_null;
    ar(is_null);
    if (!is_null) {
        std::pair<int,int> deserializer_id;
        ar(deserializer_id);
        auto deserializer = reinterpret_cast<DeserializerT>(
            get_caller_registry().get_function(deserializer_id)
        );
        deserializer(*this, ar);
    }
}

bool Data::operator==(std::nullptr_t) const { return ptr == nullptr; }
bool Data::operator!=(std::nullptr_t) const { return ptr != nullptr; }

Data ensure_data(Data& d) { return d; }
Data ensure_data(Data&& d) { return std::move(d); }

} // end namespace taskloaf 

#include "closure.hpp"

namespace taskloaf {

bool Closure::operator==(std::nullptr_t) const { return f == nullptr; }
bool Closure::operator!=(std::nullptr_t) const { return f != nullptr; }

Data Closure::operator()(Data& arg) {
    return caller(*this, arg);
}

Data Closure::operator()(Data&& arg) {
    return caller(*this, arg);
}

Data Closure::operator()() {
    Data d;
    return caller(*this, d);
}

void Closure::save(cereal::BinaryOutputArchive& ar) const {
    serializer(ar);
    ar(f);
    ar(d);
}

void Closure::load(cereal::BinaryInputArchive& ar) {
    std::pair<int,int> deserializer_id;
    ar(deserializer_id);
    auto deserializer = reinterpret_cast<DeserializerT>(
        get_caller_registry().get_function(deserializer_id)
    );
    deserializer(*this, ar);
}

} //end namespace taskloaf 

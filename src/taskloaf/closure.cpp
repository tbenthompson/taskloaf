#include "closure.hpp"

namespace taskloaf {

bool closure::operator==(std::nullptr_t) const { return f == nullptr; }
bool closure::operator!=(std::nullptr_t) const { return f != nullptr; }

Data closure::operator()(Data& arg) {
    return caller(*this, arg);
}

Data closure::operator()(Data&& arg) {
    return caller(*this, arg);
}

Data closure::operator()() {
    Data d;
    return caller(*this, d);
}

void closure::save(cereal::BinaryOutputArchive& ar) const {
    serializer(ar);
    ar(f);
    ar(d);
}

void closure::load(cereal::BinaryInputArchive& ar) {
    std::pair<int,int> deserializer_id;
    ar(deserializer_id);
    auto deserializer = reinterpret_cast<deserializer_type>(
        get_fnc_registry().get_function(deserializer_id)
    );
    deserializer(*this, ar);
}

} //end namespace taskloaf 

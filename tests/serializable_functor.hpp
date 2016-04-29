#pragma once

namespace taskloaf {

struct SerializableFunctor {
    std::vector<int> vs;

    SerializableFunctor() = default;

    int operator()(int x) const {
        int out = x;
        for (auto& v: vs) {
            out *= v; 
        }
        return out;
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(vs);
    }
};

} //end namespace taskloaf

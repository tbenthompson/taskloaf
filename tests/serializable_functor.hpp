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

inline closure get_serializable_functor() {
    SerializableFunctor s;
    s.vs = {1,2,3,4};
    return {[] (SerializableFunctor& f, int a) { return f(a); }, s};
}

} //end namespace taskloaf

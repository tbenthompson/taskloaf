#pragma once

#include "id.hpp"

#include <vector>
#include <set>

namespace taskloaf {

struct RefData {
    size_t ref_id;
    int generation;
    int children;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ref_id);
        ar(generation);
        ar(children);
    }

    bool operator<(const RefData& other) const {
        return ref_id < other.ref_id;
    }
};

RefData initial_ref();
RefData copy_ref(RefData& ref);

struct ReferenceCount {
    RefData source_ref;
    std::vector<int> counts;
    std::set<RefData> deletes;

    ReferenceCount();

    void zero();
    void merge(const ReferenceCount& other);
    void dec(const RefData& ref);
    bool alive();
    bool dead();

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(source_ref);
        ar(counts);
        ar(deletes);
    }
};

} //end namespace taskloaf

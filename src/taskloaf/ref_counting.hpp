#pragma once

#include "id.hpp"

#include <vector>
#include <set>

namespace taskloaf {

struct RefData {
    size_t ref_id;
    int generation = 0;
    int children = 0;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ref_id);
        ar(generation);
        ar(children);
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

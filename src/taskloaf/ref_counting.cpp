#include "ref_counting.hpp"

#include <cassert>

#include <iostream>

namespace taskloaf {

RefData initial_ref() {
    return {random_sizet(), 0, 0};
}

RefData copy_ref(RefData& ref) {
    ref.children++;
    return {random_sizet(), ref.generation + 1, 0};
}

ReferenceCount::ReferenceCount():
    source_ref{random_sizet(), -1, 0},
    counts(1, 1)
{}

void ReferenceCount::zero() {
    counts.clear();
}

void ReferenceCount::merge(const ReferenceCount& other) {
    for (auto& d: other.deletes) {
        if (deletes.count(d) > 0) {
            continue;
        }
        dec(d);
    }
    dec(other.source_ref);
}

void ReferenceCount::dec(const RefData& ref) {
    auto csize = static_cast<int>(counts.size());
    if (ref.children == 0 && ref.generation >= csize) {
        counts.resize(ref.generation + 1);
    } else if (ref.generation + 1 >= csize) {
        counts.resize(ref.generation + 2);
    }
    if (ref.generation >= 0) {
        counts[ref.generation]--;
    }
    counts[ref.generation + 1] += ref.children;
    deletes.insert(ref);
    assert(counts[0] >= -source_ref.children);
}

bool ReferenceCount::alive() {
    if (counts.size() == 0) {
        return false;
    }
    if (counts[0] != -source_ref.children) {
        return true;
    }
    for (size_t i = 1; i < counts.size(); i++) {
        if (counts[i] != 0) {
            return true;
        }
    }
    return false;
}

bool ReferenceCount::dead() {
    return !alive(); 
}
 
} //end namespace taskloaf

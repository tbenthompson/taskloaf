#include "ivar_tracker.hpp"
#include "id.hpp"

#include <iostream>

namespace taskloaf {

std::pair<IVarRef,bool> IVarTracker::new_ivar(Address addr, const ID& id) {
    (void)id;
    if (ref_counts.count(id) == 0) {
        ref_counts.insert({id, 0});
        return {IVarRef(addr, id), true};
    }
    return {IVarRef(addr, id), false};
}

void IVarTracker::fulfill(const IVarRef& iv, std::vector<Data> input) {
    assert(vals.size() > 0);
    assert(vals.count(iv.id) == 0);

    for (auto& t: triggers[iv.id]) {
        t(input);
    }
    triggers[iv.id].clear();
    vals[iv.id] = std::move(input);
}

void IVarTracker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    if (vals.count(iv.id) > 0) {
        trigger(vals[iv.id]);
    } else {
        triggers[iv.id].push_back(std::move(trigger));
    }
}

void IVarTracker::inc_ref(const IVarRef& iv) {
    assert(ivars.count(iv.id) > 0);
    ref_counts[iv.id]++;
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    assert(ivars.count(iv.id) > 0);
    auto& ref_count = ref_counts[iv.id];
    ref_count--;
    if (ref_count <= 0) {
        // std::cout << "erasing " << iv.id << std::endl;
        triggers.erase(iv.id);
        vals.erase(iv.id);
        ref_counts.erase(iv.id);
    }
}

} //end namespace taskloaf

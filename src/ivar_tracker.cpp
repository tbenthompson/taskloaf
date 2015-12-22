#include "ivar_tracker.hpp"

namespace taskloaf {

IVarTracker::IVarTracker(Address addr):
    addr(addr),
    next_ivar_id(0)
{}

IVarRef IVarTracker::new_ivar() {
    auto id = next_ivar_id;
    next_ivar_id++;
    ivars.insert({id, {}});
    return IVarRef(addr, id);
}

void IVarTracker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    assert(vals.size() > 0);
    auto& ivar_data = ivars[iv.id];
    assert(ivar_data.vals.size() == 0);

    ivar_data.vals = std::move(vals);
    for (auto& t: ivar_data.fulfill_triggers) {
        t(ivar_data.vals);
    }
    ivar_data.fulfill_triggers.clear();
}

void IVarTracker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    auto& ivar_data = ivars[iv.id];
    if (ivar_data.vals.size() > 0) {
        trigger(ivar_data.vals);
    } else {
        ivar_data.fulfill_triggers.push_back(std::move(trigger));
    }
}

void IVarTracker::inc_ref(const IVarRef& iv) {
    assert(ivars.count(iv.id) > 0);
    ivars[iv.id].ref_count++;
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    assert(ivars.count(iv.id) > 0);
    auto& ref_count = ivars[iv.id].ref_count;
    ref_count--;
    if (ref_count == 0) {
        ivars.erase(iv.id);
    }
}

} //end namespace taskloaf

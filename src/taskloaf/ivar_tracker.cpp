#include "ivar_tracker.hpp"

#include <iostream>

namespace taskloaf {

IVarTracker::IVarTracker():
    next_ivar_id(0)
{}

bool IVarTracker::empty() {
    return ivars.empty();
}

IVarRef IVarTracker::new_ivar(Address addr) {
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
    // std::cout << "start incref(" << iv.id << ") from " << ivars[iv.id].ref_count << std::endl;
    ivars[iv.id].ref_count++;
    // std::cout << "end incref(" << iv.id << ") to " << ivars[iv.id].ref_count << std::endl;
}

void IVarTracker::dec_ref(const IVarRef& iv) {
    assert(ivars.count(iv.id) > 0);
    // std::cout << "start decref(" << iv.id << ") from " << ivars[iv.id].ref_count << std::endl;
    auto& ref_count = ivars[iv.id].ref_count;
    ref_count--;
    // std::cout << "end decref(" << iv.id << ") to " << ivars[iv.id].ref_count << std::endl;
    if (ref_count <= 0) {
        // std::cout << "erasing " << iv.id << std::endl;
        ivars.erase(iv.id);
    }
}

} //end namespace taskloaf

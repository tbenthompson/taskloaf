#include "ivar.hpp"
#include "worker.hpp"

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>

namespace taskloaf {

ivar::ivar():
    iv(std::make_shared<ivar_data>())
{}

    
void add_trigger_helper(std::shared_ptr<ivar_data>& iv, closure& trigger) {
    if (iv->val.empty()) {
        iv->triggers.push_back(std::move(trigger));
    } else {
        trigger(iv->val);
    }
}

auto add_trigger_sendable(std::tuple<std::shared_ptr<ivar_data>,closure>& p, ignore) {
    add_trigger_helper(std::get<0>(p), std::get<1>(p));
    return ignore{};
}

void ivar::add_trigger(closure trigger) {
    if (iv->owner == cur_addr) {
        add_trigger_helper(iv, trigger); 
    } else {
        cur_worker->add_task(iv->owner, make_closure(
            add_trigger_sendable, std::make_tuple(iv, std::move(trigger))
        ));
    }
}

void fulfill_helper(std::shared_ptr<ivar_data>& iv, data_ptr val) {
    TLASSERT(iv->val.empty());
    iv->val = std::move(val);
    for (auto& t: iv->triggers) {
        t(iv->val);
    }
    iv->triggers.clear();
}

auto fulfill_sendable(std::tuple<std::shared_ptr<ivar_data>,data_ptr>& p, ignore&) {
    fulfill_helper(std::get<0>(p), std::move(std::get<1>(p)));
    return ignore{};
};

void ivar::fulfill(data_ptr val) {
    if (iv->owner == cur_addr) {
        fulfill_helper(iv, std::move(val)); 
    } else {
        cur_worker->add_task(iv->owner, make_closure(
            fulfill_sendable, std::make_tuple(iv, std::move(val))
        ));
    }
}

data_ptr& ivar::get() {
    return iv->val;
}

} //end namespace taskloaf

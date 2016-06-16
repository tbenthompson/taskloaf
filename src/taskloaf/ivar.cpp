#include "ivar.hpp"
#include "worker.hpp"

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>

namespace taskloaf {

__thread ivar_db* cur_ivar_db;

bool ivar::fulfilled_here() const {
    if (rr.local()) {
        return !rr.get().val.empty(); 
    }
    return false;
}

closure concat_closures(closure a, closure b) {
    return closure(
        [] (std::tuple<closure,closure>& p, data& v) {
            std::get<0>(p)(v);
            std::get<1>(p)(v);
            return ignore{};
        },
        std::make_tuple(std::move(a), std::move(b))
    );
}

void add_trigger_helper(ivar_data& iv, closure& trigger) {
    if (iv.val.empty()) {
        if (iv.triggers.empty()) {
            iv.triggers = std::move(trigger);
        } else {
            iv.triggers = concat_closures(std::move(iv.triggers), std::move(trigger));
        }
    } else {
        trigger(iv.val);
    }
}

auto add_trigger_sendable(std::pair<remote_ref,closure>& p, ignore) {
    add_trigger_helper(std::get<0>(p).get(), std::get<1>(p));
    return ignore{};
}

void ivar::add_trigger(closure trigger) {
    if (rr.local()) {
        add_trigger_helper(rr.get(), trigger); 
    } else {
        cur_worker->add_task(rr.owner, closure(
            add_trigger_sendable, std::make_pair(rr, std::move(trigger))
        ));
    }
}

void fulfill_helper(ivar_data& iv, data val) {
    TLASSERT(iv.val.empty());
    iv.val = std::move(val);
    if (!iv.triggers.empty()) {
        iv.triggers(iv.val);
    }
}

auto fulfill_sendable(std::pair<remote_ref,data>& p, ignore&) {
    fulfill_helper(std::get<0>(p).get(), std::move(std::get<1>(p)));
    return ignore{};
};

void ivar::fulfill(data val) {
    if (rr.local()) {
        fulfill_helper(rr.get(), std::move(val)); 
    } else {
        cur_worker->add_task(rr.owner, closure(
            fulfill_sendable, std::make_pair(rr, std::move(val))
        ));
    }
}

data ivar::get() {
    TLASSERT(fulfilled_here());
    return rr.get().val;
}

} //end namespace taskloaf

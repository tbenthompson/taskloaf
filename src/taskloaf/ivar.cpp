#include "ivar.hpp"
#include "worker.hpp"

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>

namespace taskloaf {

ivar::ivar():
    iv(ivar_data())
{}
    
void add_trigger_helper(ivar_data& iv, closure& trigger) {
    if (iv.val.empty()) {
        if (iv.triggers.empty()) {
            iv.triggers = std::move(trigger);
        } else {
            auto old_trigger = std::move(iv.triggers);
            iv.triggers = closure(
                [] (std::tuple<closure,closure>& p, data& v) {
                    std::get<0>(p)(v);
                    std::get<1>(p)(v);
                    return ignore{};
                },
                std::make_tuple(
                    std::move(old_trigger),
                    std::move(trigger)
                )
            );
        }
    } else {
        trigger(iv.val);
    }
}

auto add_trigger_sendable(std::tuple<data,closure>& p, ignore) {
    add_trigger_helper(std::get<0>(p), std::get<1>(p));
    return ignore{};
}

void ivar::add_trigger(closure trigger) {
    auto& d = iv.get<ivar_data>();
    if (d.owner == cur_addr) {
        add_trigger_helper(iv, trigger); 
    } else {
        cur_worker->add_task(d.owner, closure(
            add_trigger_sendable, std::make_tuple(iv, std::move(trigger))
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

auto fulfill_sendable(std::tuple<data,data>& p, ignore&) {
    fulfill_helper(std::get<0>(p), std::move(std::get<1>(p)));
    return ignore{};
};

void ivar::fulfill(data val) {
    auto& d = iv.get<ivar_data>();
    if (d.owner == cur_addr) {
        fulfill_helper(iv, std::move(val)); 
    } else {
        cur_worker->add_task(d.owner, closure(
            fulfill_sendable, std::make_tuple(iv, std::move(val))
        ));
    }
}

bool ivar::fulfilled() const {
    return !iv.get<ivar_data>().val.empty();
}

data& ivar::get() {
    return iv.get<ivar_data>().val;
}

} //end namespace taskloaf

#include "ivar.hpp"
#include "worker.hpp"

#include <cereal/types/memory.hpp>

namespace taskloaf {

IVar::IVar():
    data(std::make_shared<IVarData>())
{}

    
void add_trigger_helper(std::shared_ptr<IVarData>& data, closure& trigger) {
    if (data->val != nullptr) {
        trigger(data->val);
    } else {
        data->triggers.push_back(std::move(trigger));
    }
}

Data add_trigger_sendable(std::tuple<std::shared_ptr<IVarData>,closure>& p, Data&) {
    add_trigger_helper(std::get<0>(p), std::get<1>(p));
    return Data{};
}

void IVar::add_trigger(closure trigger) {
    if (data->owner == cur_addr) {
        add_trigger_helper(data, trigger); 
    } else {
        cur_worker->add_task(data->owner, closure(
            add_trigger_sendable, std::make_tuple(data, std::move(trigger))
        ));
    }
}

void fulfill_helper(std::shared_ptr<IVarData>& data, Data& val) {
    tlassert(data->val == nullptr);
    data->val = std::move(val);
    for (auto& t: data->triggers) {
        t(data->val);
    }
    data->triggers.clear();
}

Data fulfill_sendable(std::tuple<std::shared_ptr<IVarData>,Data>& p, Data&) {
    fulfill_helper(std::get<0>(p), std::get<1>(p));
    return Data{};
};

void IVar::fulfill(Data val) {
    if (data->owner == cur_addr) {
        fulfill_helper(data, val); 
    } else {
        cur_worker->add_task(data->owner, closure(
            fulfill_sendable, std::make_tuple(data, std::move(val))
        ));
    }
}

Data IVar::get() {
    return data->val;
}

} //end namespace taskloaf

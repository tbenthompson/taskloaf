#include "ivar.hpp"
#include "worker.hpp"
#include <cereal/types/memory.hpp>

namespace taskloaf {

IVar::IVar(): data(std::make_shared<IVarData>()) {
    data->owner = cur_worker->get_addr();
}

void IVar::add_trigger(TriggerT trigger) {
    auto action = [] (std::shared_ptr<IVarData>& data, 
        TriggerT& trigger) 
    {
        if (data->fulfilled) {
            trigger(data->vals);
        } else {
            data->triggers.push_back(std::move(trigger));
        }
    };
    if (data->owner == cur_worker->get_addr()) {
        action(data, trigger); 
    } else {
        cur_worker->add_task(data->owner, TaskT(
            action, data, std::move(trigger)
        ));
    }
}

void IVar::fulfill(std::vector<Data> vals) {
    auto action = [] (std::shared_ptr<IVarData>& data, std::vector<Data>& vals) {
        tlassert(!data->fulfilled);
        data->fulfilled = true;
        data->vals = std::move(vals);
        for (auto& t: data->triggers) {
            t(data->vals);
        }
        data->triggers.clear();
    };
    if (data->owner == cur_worker->get_addr()) {
        action(data, vals); 
    } else {
        cur_worker->add_task(data->owner, TaskT(
            action, data, std::move(vals)
        ));
    }
}

std::vector<Data> IVar::get_vals() {
    return data->vals;
}

} //end namespace taskloaf

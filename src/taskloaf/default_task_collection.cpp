#include "default_task_collection.hpp"
#include "comm.hpp"
#include "protocol.hpp"
#include "address.hpp"
#include "logging.hpp"
#include <cereal/types/utility.hpp>

#include <iostream>
#include <cmath>

namespace taskloaf {

DefaultTaskCollection::DefaultTaskCollection(Log& log, Comm& comm):
    log(log),
    comm(comm),
    stealing(false)
{
    comm.add_handler(Protocol::Steal, [&] (Data d) {
        auto sender = d.get_as<Address>();
        auto response = victimized();
        comm.send(sender, Msg(Protocol::StealResponse, make_data(std::move(response))));
    });

    comm.add_handler(Protocol::StealResponse, [&] (Data d) {
        receive_tasks(std::move(d.get_as<std::vector<TaskT>>()));
    });

    comm.add_handler(Protocol::AssignedTask, [&] (Data d) {
        add_local_task(std::move(d.get_as<TaskT>()));
    });
}

size_t DefaultTaskCollection::size() const {
    return stealable_tasks.size() + local_tasks.size();
}

void DefaultTaskCollection::add_task(TaskT t) {
    stealable_tasks.push_front(std::make_pair(next_token, std::move(t)));
    next_token++;
}

void DefaultTaskCollection::add_local_task(TaskT t) {
    local_tasks.push(std::make_pair(next_token, std::move(t)));
    next_token++;
}

void DefaultTaskCollection::add_task(const Address& where, TaskT t) {
    if (where != comm.get_addr()) {
        comm.send(where, Msg(Protocol::AssignedTask, make_data(std::move(t))));
    } else {
        add_local_task(std::move(t));
    }
}

void DefaultTaskCollection::run_next() {
    tlassert(size() > 0);
    TaskT t;
    auto grab_local_task = [&] () {
        t = std::move(local_tasks.top().second);
        local_tasks.pop();
    };
    auto grab_stealable_task = [&] () {
        t = std::move(stealable_tasks.front().second);
        stealable_tasks.pop_front();
    };

    if (local_tasks.size() == 0) {
        grab_stealable_task();
    } else if (stealable_tasks.size() == 0) {
        grab_local_task();
    } else if (stealable_tasks.front().first > local_tasks.top().first) {
        grab_stealable_task();
    } else {
        grab_local_task();
    }

    t();
    log.n_tasks_run++;
}

void DefaultTaskCollection::steal() {
    if (stealing) {
        return;
    }
    stealing = true;
    log.n_attempted_steals++;
    comm.send_random(Msg(Protocol::Steal, make_data(comm.get_addr())));
}

std::vector<TaskT> DefaultTaskCollection::victimized() {
    log.n_victimized++;
    auto n_steals = std::min(static_cast<size_t>(1), stealable_tasks.size());
    tlassert(n_steals <= stealable_tasks.size());
    std::vector<TaskT> steals;
    for (size_t i = 0; i < n_steals; i++) {
        steals.push_back(std::move(stealable_tasks.back().second));
        stealable_tasks.pop_back();
    }
    return steals;
}

void DefaultTaskCollection::receive_tasks(std::vector<TaskT> stolen_tasks) {
    if (stolen_tasks.size() > 0) {
        log.n_successful_steals++;
    }
    for (auto& t: stolen_tasks) {
        stealable_tasks.push_back(std::make_pair(next_token, std::move(t)));
        next_token++;
    }
    stealing = false;
}

}//end namespace taskloaf

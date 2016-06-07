#include "task_collection.hpp"
#include "default_worker.hpp"
#include "comm.hpp"
#include "address.hpp"
#include "logging.hpp"
#include <cereal/types/utility.hpp>

#include <iostream>
#include <random>

namespace taskloaf {

TaskCollection::TaskCollection(Log& log, Comm& comm):
    log(log),
    comm(comm),
    stealing(false)
{}

size_t TaskCollection::size() const {
    return stealable_tasks.size() + local_tasks.size();
}

void TaskCollection::add_task(TaskT t) {
    stealable_tasks.push_front(std::make_pair(next_token, std::move(t)));
    next_token++;
}

void TaskCollection::add_local_task(TaskT t) {
    local_tasks.push(std::make_pair(next_token, std::move(t)));
    next_token++;
}

void TaskCollection::add_task(const Address& where, TaskT t) {
    if (where != comm.get_addr()) {
        comm.send(where, std::move(t));
    } else {
        add_local_task(std::move(t));
    }
}

void TaskCollection::run_next() {
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

void TaskCollection::steal() {
    if (stealing) {
        return;
    }
    stealing = true;
    log.n_attempted_steals++;

    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    auto& remotes = comm.remote_endpoints();
    if (remotes.size() == 0) {
        return;
    }
    std::uniform_int_distribution<> dis(0, remotes.size() - 1);
    auto idx = dis(gen);

    add_task(remotes[idx], TaskT(
        [] (const Address& sender) {
            auto& tc = ((DefaultWorker*)(cur_worker))->tasks;
            auto response = tc.victimized();

            tc.add_task(sender, TaskT(
                [] (std::vector<TaskT>& tasks) {
                    auto& tc = ((DefaultWorker*)(cur_worker))->tasks;
                    tc.receive_tasks(std::move(tasks));
                },
                std::move(response)
            ));
        },
        comm.get_addr()
    ));
}

std::vector<TaskT> TaskCollection::victimized() {
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

void TaskCollection::receive_tasks(std::vector<TaskT> stolen_tasks) {
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

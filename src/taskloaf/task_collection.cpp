#include "task_collection.hpp"
#include "default_worker.hpp"
#include "comm.hpp"
#include "address.hpp"
#include "logging.hpp"

#include <cereal/types/vector.hpp>

#include <iostream>
#include <random>

namespace taskloaf {

task_collection::task_collection(logger& log, comm& my_comm):
    log(log),
    my_comm(my_comm)
{}

size_t task_collection::size() const {
    return stealable_tasks.size() + local_tasks.size();
}

void task_collection::add_task(closure t) {
    if (running_task) {
        temp_tasks.push_back(std::move(t));
    } else {
        stealable_tasks.push_front(std::make_pair(next_token, std::move(t)));
    }
    next_token++;
}

void task_collection::add_local_task(closure t) {
    local_tasks.push(std::make_pair(next_token, std::move(t)));
    next_token++;
}

void task_collection::add_task(const address& where, closure t) {
    if (where != my_comm.get_addr()) {
        my_comm.send(where, std::move(t));
    } else {
        add_local_task(std::move(t));
    }
}

void task_collection::run(closure t) {
    running_task = true;
    TLASSERT(temp_tasks.size() == 0);
    t();
    running_task = false;
    while (temp_tasks.size() > 0) {
        add_task(std::move(temp_tasks.back()));
        temp_tasks.pop_back();
    }
}

void task_collection::run_next() {
    TLASSERT(size() > 0);
    closure t;
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

    run(std::move(t));
    log.n_tasks_run++;
}

void task_collection::steal() {
    if (stealing) {
        return;
    }
    stealing = true;
    log.n_attempted_steals++;

    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    auto& remotes = my_comm.remote_endpoints();
    if (remotes.size() == 0) {
        return;
    }
    std::uniform_int_distribution<> dis(0, remotes.size() - 1);
    auto idx = dis(gen);

    add_task(remotes[idx], closure(
        [] (const address& sender, _) {
            auto& tc = ((default_worker*)(cur_worker))->tasks;
            auto response = tc.victimized();

            tc.add_task(sender, closure(
                [] (std::vector<closure>& tasks, _) {
                    auto& tc = ((default_worker*)(cur_worker))->tasks;
                    tc.receive_tasks(std::move(tasks));
                    return _{};
                },
                std::move(response)
            ));
            return _{};
        },
        my_comm.get_addr()
    ));
}

std::vector<closure> task_collection::victimized() {
    log.n_victimized++;
    auto n_steals = std::min(static_cast<size_t>(1), stealable_tasks.size());
    TLASSERT(n_steals <= stealable_tasks.size());
    std::vector<closure> steals;
    for (size_t i = 0; i < n_steals; i++) {
        steals.push_back(std::move(stealable_tasks.back().second));
        stealable_tasks.pop_back();
    }
    return steals;
}

void task_collection::receive_tasks(std::vector<closure> stolen_tasks) {
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

#pragma once

#include "worker.hpp"
#include <deque>
#include <stack>

namespace taskloaf {

struct comm;
struct log;

struct task_collection {
    log& my_log;
    comm& my_comm;
    std::deque<std::pair<int,closure>> stealable_tasks;
    std::stack<std::pair<int,closure>> local_tasks;
    int next_token = 0;
    bool stealing;

    task_collection(log& log, comm& comm);

    size_t size() const;
    void add_task(closure t);
    void add_task(const address& where, closure t);
    void run_next();
    void steal();

    void add_local_task(closure t);
    std::vector<closure> victimized();
    void receive_tasks(std::vector<closure> stolen_tasks);
};

} //end namespace taskloaf 

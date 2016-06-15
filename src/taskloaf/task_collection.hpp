#pragma once

#include "worker.hpp"

#include "short_alloc.h"
#include <deque>
#include <stack>

namespace taskloaf {

struct comm;
struct logger;

struct task_collection {
    using task_type = std::pair<size_t,closure>;

    logger& log;
    comm& my_comm;

    std::deque<task_type> stealable_tasks;
    std::stack<task_type> local_tasks;

    size_t next_token = 0;
    bool stealing;

    task_collection(logger& log, comm& comm);

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

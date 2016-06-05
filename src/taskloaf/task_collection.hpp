#pragma once

#include "worker.hpp"
#include <deque>
#include <stack>

namespace taskloaf {

struct Comm;
struct Log;

struct TaskCollection {
    Log& log;
    Comm& comm;
    std::deque<std::pair<int,TaskT>> stealable_tasks;
    std::stack<std::pair<int,TaskT>> local_tasks;
    int next_token = 0;
    bool stealing;

    TaskCollection(Log& log, Comm& comm);

    size_t size() const;
    void add_task(TaskT t);
    void add_task(const Address& where, TaskT t);
    void run_next();
    void steal();

    void add_local_task(TaskT t);
    std::vector<TaskT> victimized();
    void receive_tasks(std::vector<TaskT> stolen_tasks);
};

} //end namespace taskloaf 

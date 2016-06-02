#include "task_collection.hpp"

#include <deque>
#include <stack>

#pragma once

namespace taskloaf {

struct Comm;
struct Log;

struct DefaultTaskCollection: public TaskCollection {
    Log& log;
    Comm& comm;
    std::deque<std::pair<int,TaskT>> stealable_tasks;
    std::stack<std::pair<int,TaskT>> local_tasks;
    int next_token = 0;
    bool stealing;

    DefaultTaskCollection(Log& log, Comm& comm);

    size_t size() const override;
    void add_task(TaskT t) override;
    void add_task(const Address& where, TaskT t) override;
    void run_next() override;
    void steal() override;

    void add_local_task(TaskT t);
    std::vector<TaskT> victimized();
    void receive_tasks(std::vector<TaskT> stolen_tasks);
};

} //end namespace taskloaf 

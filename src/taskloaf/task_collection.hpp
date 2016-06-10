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
    std::deque<std::pair<int,closure>> stealable_tasks;
    std::stack<std::pair<int,closure>> local_tasks;
    int next_token = 0;
    bool stealing;

    TaskCollection(Log& log, Comm& comm);

    size_t size() const;
    void add_task(closure t);
    void add_task(const Address& where, closure t);
    void run_next();
    void steal();

    void add_local_task(closure t);
    std::vector<closure> victimized();
    void receive_tasks(std::vector<closure> stolen_tasks);
};

} //end namespace taskloaf 

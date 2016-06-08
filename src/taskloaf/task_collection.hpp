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
    std::deque<std::pair<int,Closure>> stealable_tasks;
    std::stack<std::pair<int,Closure>> local_tasks;
    int next_token = 0;
    bool stealing;

    TaskCollection(Log& log, Comm& comm);

    size_t size() const;
    void add_task(Closure t);
    void add_task(const Address& where, Closure t);
    void run_next();
    void steal();

    void add_local_task(Closure t);
    std::vector<Closure> victimized();
    void receive_tasks(std::vector<Closure> stolen_tasks);
};

} //end namespace taskloaf 

#pragma once
#include "closure.hpp"

#include <deque>

namespace taskloaf {

struct Comm;
struct Log;
struct TaskCollection {
    Log& log;
    Comm& comm;
    std::deque<TaskT> tasks;
    bool stealing;

    TaskCollection(Log& log, Comm& comm);

    size_t size() const;
    void add_task(TaskT f);
    void run_next();
    void steal();
};

} //end namespace taskloaf

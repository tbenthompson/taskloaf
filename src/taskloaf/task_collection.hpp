#pragma once
#include "closure.hpp"

#include <deque>

namespace taskloaf {

struct Comm;
struct TaskCollection {
    std::deque<TaskT> tasks;
    Comm& comm;
    bool stealing;

    TaskCollection(Comm& comm);

    size_t size() const;
    void add_task(TaskT f);
    TaskT next();
    void steal();
};

} //end namespace taskloaf

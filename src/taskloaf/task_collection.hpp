#pragma once
#include "fnc.hpp"

#include <deque>

namespace taskloaf {

typedef Function<void()> TaskT;

struct TaskCollection {
    std::deque<TaskT> tasks;

    size_t size() const;
    bool allow_stealing(size_t n_remote_tasks) const;
    void stolen_task(TaskT f);
    void add_task(TaskT f);
    TaskT next();
    TaskT steal();
};

} //end namespace taskloaf

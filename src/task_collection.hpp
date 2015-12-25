#pragma once
#include "fnc.hpp"

#include <deque>

namespace taskloaf {

typedef Function<void()> TaskT;

struct TaskCollection {
    std::deque<TaskT> tasks;

    size_t size();
    bool should_steal();
    bool should_allow_steal();
    void stolen_task(TaskT f);
    void add_task(TaskT f);
    TaskT next();
    TaskT steal();
};

} //end namespace taskloaf

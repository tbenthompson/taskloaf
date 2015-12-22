#pragma once
#include "fnc.hpp"

#include <stack>

namespace taskloaf {

typedef Function<void()> TaskT;

struct TaskCollection {
    std::stack<TaskT> tasks;

    void add_task(TaskT f);
    TaskT next();
    bool empty();
    //steal
};

} //end namespace taskloaf

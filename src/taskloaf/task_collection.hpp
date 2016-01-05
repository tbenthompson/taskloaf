#pragma once
#include "fnc.hpp"

#include <deque>

namespace taskloaf {

typedef Function<void()> TaskT;

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

#pragma once
#include "fnc.hpp"

#include <deque>

namespace taskloaf {

struct TaskT {
    Function<void()> fnc;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(fnc);
    }
};

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

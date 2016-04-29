#pragma once

#include "task_collection.hpp"
#include "ivar_tracker.hpp"

namespace taskloaf {

struct WorkerInternals {
    std::unique_ptr<Comm> comm;
    TaskCollection tasks;
    IVarTracker ivar_tracker;
    int core_id = -1;
    bool stealing = false;
    bool stop = false;
    int immediate_computes = 0;
    static const int immediates_before_comm = 8;
    static const int immediates_before_tasks = 100;

    WorkerInternals(std::unique_ptr<Comm> p_comm):
        comm(std::move(p_comm)),
        tasks(*comm),
        ivar_tracker(*comm)
    {}
};

} //end namespace taskloaf

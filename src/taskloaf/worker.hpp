#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"
#include "ivar_tracker.hpp"

#include <memory>

namespace taskloaf { 

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual bool can_compute_immediately() = 0;
    virtual size_t n_workers() const = 0;
    virtual void add_task(TaskT f) = 0;
    virtual void yield() = 0;
    virtual IVarTracker& get_ivar_tracker() = 0;
};

thread_local extern Worker* cur_worker;

bool can_run_immediately(Worker* w);
void yield();

} //end namespace taskloaf

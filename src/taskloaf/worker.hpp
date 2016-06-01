#pragma once
#include "address.hpp"

#include <memory>

namespace taskloaf { 

enum class Loc {
    anywhere, here
};

struct IVarTracker;
struct TaskCollection;

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual void stop() = 0;
    virtual void run() = 0;
    virtual const Address& get_addr() const = 0;
    virtual size_t n_workers() const = 0;
    virtual TaskCollection& get_task_collection() = 0;
    virtual IVarTracker& get_ref_tracker() = 0;
};

thread_local extern Worker* cur_worker;

} //end namespace taskloaf

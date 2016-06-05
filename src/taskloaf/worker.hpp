#pragma once
#include "address.hpp"
#include "closure.hpp"

#include <memory>

namespace taskloaf { 

typedef Closure<void()> TaskT;

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual void set_stopped(bool) = 0;
    virtual void run() = 0;

    virtual const Address& get_addr() const = 0;
    virtual size_t n_workers() const = 0;

    virtual void add_task(TaskT t) = 0;
    virtual void add_task(const Address& where, TaskT t) = 0;
};

thread_local extern Worker* cur_worker;

} //end namespace taskloaf

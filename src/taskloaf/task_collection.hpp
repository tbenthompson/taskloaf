#pragma once
#include "closure.hpp"

namespace taskloaf {

struct Address;

struct TaskCollection {
    virtual size_t size() const = 0;
    virtual void add_task(TaskT t) = 0;
    virtual void add_task(const Address& where, TaskT t) = 0;
    virtual void run_next() = 0;
    virtual void steal() = 0;
};

} //end namespace taskloaf

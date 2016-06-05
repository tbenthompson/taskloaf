#pragma once
#include "address.hpp"
#include "task_collection.hpp"

#include <memory>

namespace taskloaf { 

enum class Loc: int {
    anywhere = -2,
    here = -1
};

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual void set_stopped(bool) = 0;
    virtual void run() = 0;
    virtual const Address& get_addr() const = 0;
    virtual size_t n_workers() const = 0;
    virtual TaskCollection& get_task_collection() = 0;
};

thread_local extern Worker* cur_worker;

} //end namespace taskloaf

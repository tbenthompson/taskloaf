#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"

#include <memory>

namespace taskloaf { 

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual bool can_compute_immediately() = 0;
    virtual size_t n_workers() const = 0;
    virtual void add_task(TaskT f, bool push) = 0;
    virtual void fulfill(const IVarRef& ivar, std::vector<Data> vals) = 0;
    virtual void add_trigger(const IVarRef& ivar, TriggerT trigger) = 0;
    virtual void dec_ref(const IVarRef& ivar) = 0;
};

extern thread_local Worker* cur_worker;

bool can_run_immediately();


} //end namespace taskloaf

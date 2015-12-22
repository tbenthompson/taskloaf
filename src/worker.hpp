#pragma once

#include "task_collection.hpp"
#include "ivar_tracker.hpp"

//TODO: At some point when things are settled, pimpl this so that users
//don't need to include info about the internals.

namespace taskloaf { 

struct CommunicatorI;

struct Worker {
    std::unique_ptr<CommunicatorI> comm;
    TaskCollection tasks;
    IVarTracker ivars;

    Worker();
    ~Worker();

    void run();

    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);
};

extern thread_local Worker* cur_worker;

} //end namespace taskloaf

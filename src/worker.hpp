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

    void meet(Address addr);
    Address get_addr();

    void add_task(TaskT f);

    IVarRef new_ivar(); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);

    void run();
};

extern thread_local Worker* cur_worker;

} //end namespace taskloaf

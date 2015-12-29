#pragma once

#include "task_collection.hpp"
#include "ivar_tracker.hpp"

namespace taskloaf { 

struct CommunicatorI;

struct Worker {
    TaskCollection tasks;
    IVarTracker ivars;
    std::unique_ptr<CommunicatorI> comm;
    int core_id;
    bool stop;

    Worker();
    Worker(Worker&&);
    ~Worker();

    void shutdown();
    void meet(Address addr);
    Address get_addr();

    void add_task(TaskT f);

    IVarRef new_ivar(); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);

    void run();

    void set_core_affinity(int core_id);
};

extern thread_local Worker* cur_worker;

} //end namespace taskloaf

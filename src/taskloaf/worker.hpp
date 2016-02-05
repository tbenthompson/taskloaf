#pragma once

#include "task_collection.hpp"
#include "ivar_tracker.hpp"

namespace taskloaf { 

struct Comm;
struct Worker {
    std::unique_ptr<Comm> comm;
    TaskCollection tasks;
    IVarTracker ivar_tracker;
    int core_id = -1;
    bool stealing = false;
    bool stop = false;

    Worker();
    Worker(Worker&&) = delete;
    Worker(const Worker&) = delete;
    ~Worker();

    void shutdown();
    void introduce(Address addr);
    const Address& get_addr();
    void add_task(TaskT f);
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void dec_ref(const IVarRef& ivar);
    void recv();
    void run();
    void set_core_affinity(int core_id);
};

extern thread_local Worker* cur_worker;

} //end namespace taskloaf

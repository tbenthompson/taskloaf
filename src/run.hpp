#pragma once
#include "future.hpp"
#include "fnc.hpp"
#include "ivar.hpp"

#include <stack>
#include <unordered_map>

namespace taskloaf {
//TODO: At some point when things are settled, pimpl this so that users
//don't need to include info about the internals.

typedef Function<void()> TaskT;

struct Communicator;

struct Worker {
    std::stack<TaskT> tasks;
    std::unordered_map<size_t,IVarData> ivars;
    size_t next_ivar_id = 0;
    std::unique_ptr<Communicator> comm;

    Worker();
    ~Worker();

    IVarRef new_ivar(); 
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void inc_ref(const IVarRef& ivar);
    void dec_ref(const IVarRef& ivar);

    void add_task(TaskT f);
    void run();
};

extern thread_local Worker* cur_worker;

IVarRef run_helper(const FutureNode& data);

template <typename T>
void run(const Future<T>& fut, Worker& w) {
    cur_worker = &w;
    run_helper(*fut.data.get());
    w.run();
    cur_worker = nullptr;
}

}

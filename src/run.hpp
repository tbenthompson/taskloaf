#pragma once
#include "future.hpp"
#include "fnc.hpp"
#include "id.hpp"

#include <stack>
#include <unordered_map>

namespace taskloaf {

struct Worker;

typedef Function<void(std::vector<Data>& val)> TriggerT;
typedef Function<Data(std::vector<Data>&)> PureTaskT;
typedef Function<void()> TaskT;

struct IVarData {
    std::vector<Data> vals;
    std::vector<TriggerT> fulfill_triggers;
    size_t ref_count = 0;
};

struct IVarRef {
    int owner;
    size_t id;

    IVarRef(int owner, size_t id);
    IVarRef(IVarRef&&);
    IVarRef(const IVarRef&);
    IVarRef& operator=(IVarRef&&) = delete;
    IVarRef& operator=(const IVarRef&) = delete;
    ~IVarRef();

    void inc_ref();
    void dec_ref();
    void fulfill(std::vector<Data> val); 
    void add_trigger(TriggerT trigger);
};

struct Worker {
    std::stack<TaskT> tasks;
    std::unordered_map<size_t,IVarData> ivars;
    size_t next_ivar_id = 0;

    IVarRef new_ivar(); 

    void add_task(TaskT f);
    void run();
};

thread_local Worker* cur_worker;

IVarRef run_helper(const FutureNode& data);

template <typename T>
void run(const Future<T>& fut, Worker& w) {
    cur_worker = &w;
    run_helper(*fut.data.get());
    w.run();
    cur_worker = nullptr;
}

}

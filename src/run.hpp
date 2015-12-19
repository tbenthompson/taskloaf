#pragma once
#include "future.hpp"
#include "id.hpp"

#include <stack>
#include <unordered_map>

namespace taskloaf {

struct Scheduler;

typedef Function<void(Scheduler* s, std::vector<Data>& val)> TriggerT;
struct IVarData {
    std::vector<Data> vals;
    std::vector<TriggerT> fulfill_triggers;
    size_t ref_count = 0;
};

struct IVarRef {
    int owner;
    size_t id;
    Scheduler* s;

    
    IVarRef(int owner, size_t id, Scheduler* s);
    IVarRef(IVarRef&&) = default;
    IVarRef& operator=(IVarRef&&) = default;
    IVarRef(const IVarRef&);
    IVarRef& operator=(const IVarRef&);
    ~IVarRef();

    void fulfill(std::vector<Data> val); 
    void add_trigger(TriggerT trigger);
};

typedef Function<void(Scheduler*)> TaskT;
struct Scheduler {
    std::stack<TaskT> tasks;
    std::unordered_map<size_t,IVarData> ivars;
    size_t next_ivar_id = 0;

    void run();

    IVarRef new_ivar() {
        auto id = next_ivar_id;
        next_ivar_id++;
        return IVarRef(0, id, this);
    }

    template <typename F>
    void add_task(F f) 
    {
        tasks.push(std::move(f));
    }
};

IVarRef run_helper(const FutureNode& data, Scheduler* s);

template <typename T>
void run(const Future<T>& fut, Scheduler& s) {
    run_helper(*fut.data.get(), &s);
    s.run();
}

}

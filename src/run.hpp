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
};

struct IVar {
    std::shared_ptr<IVarData> data;

    IVar();
    void fulfill(Scheduler* s, std::vector<Data> val); 
    void add_trigger(Scheduler* s, TriggerT trigger);
};

typedef Function<void(Scheduler*)> TaskT;
struct Scheduler {
    std::stack<TaskT> tasks;
    std::unordered_map<ID,IVarData> ivars;

    void run();

    ID new_ivar() {
        auto id = new_id();
        ivars.insert({id, {}});
        return id;
    }

    template <typename F>
    void add_task(F f) 
    {
        tasks.push(f);
    }
};

IVar run_helper(const FutureNode& data, Scheduler* s);

template <typename T>
void run(const Future<T>& fut, Scheduler& s) {
    run_helper(*fut.data.get(), &s);
    s.run();
}

}

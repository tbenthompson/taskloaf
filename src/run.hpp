#pragma once
#include "future.hpp"

#include <stack>

namespace taskloaf {

struct Scheduler {
    std::stack<Function<void()>> tasks;

    void run();

    template <typename F>
    void add_task(F f) 
    {
        tasks.push(f);
    }
};

struct STFData {
    std::vector<Data> vals;
    std::vector<Function<void(std::vector<Data>& val)>> fulfill_triggers;
};

struct STF {
    std::shared_ptr<STFData> data;

    STF();
    void fulfill(std::vector<Data> val); 
    void add_trigger(Function<void(std::vector<Data>& val)> trigger);
};

STF run_helper(const FutureNode& data, Scheduler* s);

template <typename T>
void run(const Future<T>& fut, Scheduler& s) {
    run_helper(*fut.data.get(), &s);
    s.run();
}

}

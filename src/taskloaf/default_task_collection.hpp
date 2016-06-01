#include "task_collection.hpp"

#include <deque>
#include <stack>

#pragma once

namespace taskloaf {

struct Comm;
struct Log;

struct DefaultTaskCollection: public TaskCollection {
    Log& log;
    Comm& comm;
    std::deque<std::pair<int,TaskT>> stealable_tasks;
    std::stack<std::pair<int,TaskT>> local_tasks;
    int next_token = 0;
    bool stealing;

    DefaultTaskCollection(Log& log, Comm& comm);

    size_t size() const override;
    void add_task(TaskT t) override;
    void add_task(const Address& where, TaskT t) override;
    void run_next() override;
    void steal() override;

    void add_local_task(TaskT t);
    std::vector<TaskT> victimized();
    void receive_tasks(std::vector<TaskT> stolen_tasks);
};

struct TaskBase {
    typedef std::unique_ptr<TaskBase> Ptr;
    virtual void operator()() = 0;
    virtual Closure<void()> to_serializable() = 0;
};

template <typename F>
struct LambdaTask {
    F f;
    void operator()() {
        f();
    }
    void to_serializable() {
        return Closure<void()>{f, {}};
    }
};

struct Task {
    std::unique_ptr<TaskBase> ptr = nullptr;
    Closure<void()> serializable;

    void promote() {
        if (ptr == nullptr) {
            return;
        }
        serializable = ptr->to_serializable();
        ptr = nullptr;
    }

    void operator()() {
        if (ptr != nullptr) {
            ptr->operator()();
        } else {
            serializable();
        }
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        const_cast<Task*>(this)->promote();
        ar(serializable);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ptr = nullptr; 
        ar(serializable);
    }
};

} //end namespace taskloaf 

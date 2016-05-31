#pragma once
#include "closure.hpp"
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>

#include <deque>
#include <stack>

namespace taskloaf {

struct Comm;
struct Log;
struct Address;

struct TaskCollection {
    Log& log;
    Comm& comm;
    std::deque<std::pair<int,TaskT>> stealable_tasks;
    std::stack<std::pair<int,TaskT>> local_tasks;
    int next_token = 0;
    bool stealing;

    TaskCollection(Log& log, Comm& comm);

    size_t size() const;
    void add_task(TaskT t);
    void add_local_task(TaskT t);
    void run_next();
    void steal();
    std::vector<TaskT> victimized();
    void receive_tasks(std::vector<TaskT> stolen_tasks);
};

// struct TaskBase {
//     typedef std::unique_ptr<TaskBase> Ptr;
//     virtual void operator()() = 0;
//     virtual Closure<void()> to_serializable() = 0;
// };
// 
// template <typename F>
// struct LambdaTask {
//     F f;
//     void operator()() {
//         f();
//     }
//     void to_serializable() {
//         return Closure<void()>{f, {}};
//     }
// };
// 
// struct Task {
//     mutable std::unique_ptr<TaskBase> ptr = nullptr;
//     mutable Closure<void()> serializable;
// 
//     void promote() const {
//         if (ptr == nullptr) {
//             return;
//         }
//         serializable = ptr->to_serializable();
//         ptr = nullptr;
//     }
// 
//     void operator()() {
//         if (ptr != nullptr) {
//             ptr->operator()();
//         } else {
//             serializable();
//         }
//     }
// 
//     void save(cereal::BinaryOutputArchive& ar) const {
//         promote();
//         ar(serializable);
//     }
// 
//     void load(cereal::BinaryInputArchive& ar) {
//         ptr = nullptr; 
//         ar(serializable);
//     }
// };

} //end namespace taskloaf

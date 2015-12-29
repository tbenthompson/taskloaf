#include "task_collection.hpp"

namespace taskloaf {

size_t TaskCollection::size() const {
    return tasks.size();
}

size_t TaskCollection::steal_count(size_t n_remote_tasks) const {
    if (tasks.size() > n_remote_tasks + 1) {
        return (tasks.size() - n_remote_tasks - 1) / 4;
    } else {
        return 0;
    }
}

void TaskCollection::stolen_task(TaskT f) {
    tasks.push_back(std::move(f));
}

void TaskCollection::add_task(TaskT f) {
    tasks.push_front(std::move(f));
}

TaskT TaskCollection::next() {
    auto t = std::move(tasks.front());
    tasks.pop_front();
    return std::move(t);
}

TaskT TaskCollection::steal() {
    auto t = std::move(tasks.back());
    tasks.pop_back();
    return std::move(t);
}

}//end namespace taskloaf

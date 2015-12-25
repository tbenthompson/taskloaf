#include "task_collection.hpp"

namespace taskloaf {

size_t TaskCollection::size() {
    return tasks.size();
}

bool TaskCollection::should_steal() {
    return tasks.size() < 2;
}

bool TaskCollection::should_allow_steal() {
    return tasks.size() >= 3;
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

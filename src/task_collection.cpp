#include "task_collection.hpp"

namespace taskloaf {

bool TaskCollection::empty() {
    return tasks.empty();
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

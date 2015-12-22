#include "task_collection.hpp"

namespace taskloaf {

void TaskCollection::add_task(TaskT f) {
    tasks.push(std::move(f));
}

TaskT TaskCollection::next() {
    auto t = std::move(tasks.top());
    tasks.pop();
    return std::move(t);
}

bool TaskCollection::empty() {
    return tasks.empty();
}

}//end namespace taskloaf

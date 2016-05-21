#include "worker.hpp"
#include <thread>

namespace taskloaf {

thread_local Worker* cur_worker = nullptr;

bool can_run_immediately(Worker* w) {
    return w == nullptr || w->can_compute_immediately();
}

void yield() {
    if (cur_worker != nullptr) {
        cur_worker->yield();
    }
}

} //end namespace taskloaf

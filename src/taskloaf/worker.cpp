#include "worker.hpp"
#include <thread>

namespace taskloaf {

thread_local Worker* cur_worker = nullptr;

bool can_run_immediately() {
    return cur_worker == nullptr || cur_worker->can_compute_immediately();
}

} //end namespace taskloaf

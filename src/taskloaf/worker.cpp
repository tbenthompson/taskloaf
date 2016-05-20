#include "worker.hpp"
#include <thread>

namespace taskloaf {

thread_local Worker* cur_worker;

bool can_run_immediately(Worker* w) {
    return w == nullptr || w->can_compute_immediately();
}

} //end namespace taskloaf

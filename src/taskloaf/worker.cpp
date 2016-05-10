#include "worker.hpp"

namespace taskloaf {

thread_local Worker* cur_worker;

bool can_run_immediately() {
    return cur_worker == nullptr || cur_worker->can_compute_immediately();
}

} //end namespace taskloaf

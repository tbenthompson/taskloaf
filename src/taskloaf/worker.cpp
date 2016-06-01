#include "worker.hpp"
#include <thread>

namespace taskloaf {

thread_local Worker* cur_worker = nullptr;

} //end namespace taskloaf

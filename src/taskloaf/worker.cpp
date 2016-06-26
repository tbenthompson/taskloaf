#include "worker.hpp"
#include <thread>

namespace taskloaf {

thread_local worker* cur_worker;

} //end namespace taskloaf

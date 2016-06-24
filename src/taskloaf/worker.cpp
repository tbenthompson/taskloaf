#include "worker.hpp"
#include <thread>

namespace taskloaf {

__thread worker* cur_worker;

} //end namespace taskloaf

#include "worker.hpp"
#include <thread>

namespace taskloaf {

__thread Worker* cur_worker;
__thread Address cur_addr;

void set_cur_worker(Worker* w) {
    cur_worker = w;
    cur_addr = w->get_addr();
}

void clear_cur_worker() {
    cur_worker = nullptr;
    cur_addr = {0};
}

} //end namespace taskloaf

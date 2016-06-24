#pragma once
#include "taskloaf/worker.hpp"

namespace taskloaf {
struct fake_worker: public worker {
    address addr;

    fake_worker(int addr): addr{addr} {}

    void shutdown() {}
    void set_stopped(bool) {}
    void run() {}
    const address& get_addr() const { return addr; }
    size_t n_workers() const { return 0; }
    void add_task(closure) {}
    void add_task(const address&, closure) {}
};
}

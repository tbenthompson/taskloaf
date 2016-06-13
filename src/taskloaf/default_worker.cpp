#include "default_worker.hpp"
#include "comm.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

default_worker::default_worker(std::unique_ptr<comm> p_comm):
    my_comm(std::move(p_comm)),
    my_log(my_comm->get_addr()),
    tasks(my_log, *my_comm),
    should_stop(false)
{}

default_worker::~default_worker() {
    set_stopped(true);
}

void default_worker::shutdown() {
    auto& remotes = my_comm->remote_endpoints();
    for (auto& r: remotes) {
        add_task(r, make_closure([] (ignore, ignore) {
            cur_worker->set_stopped(true); return ignore{}; 
        }));
    }
    set_stopped(true);
}

void default_worker::set_stopped(bool val) {
    should_stop = val;
}

void default_worker::add_task(closure t) {
    tasks.add_task(std::move(t));
}

void default_worker::add_task(const address& where, closure t) {
    tasks.add_task(where, std::move(t));
}

size_t default_worker::n_workers() const {
    return my_comm->remote_endpoints().size() + 1;
}

const address& default_worker::get_addr() const {
    return my_comm->get_addr();
}

bool default_worker::is_stopped() const {
    return should_stop;
}

comm& default_worker::get_comm() {
    return *my_comm;
}

void default_worker::recv() {
    my_comm->recv();
}

void default_worker::one_step() {
    auto t = my_comm->recv();
    if (!t.empty()) {
        tasks.add_local_task(std::move(t));
    }

    if (tasks.size() == 0) {
        tasks.steal();
    } else {
        tasks.run_next();
    }
}

void default_worker::run() {
    while (!is_stopped()) {
        one_step();
    }
}

void default_worker::set_core_affinity(int core_id) {
    this->core_id = core_id;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(core_id, &cs);
    auto err = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    (void)err;
    TLASSERT(err == 0);
}

} //end namespace taskloaf

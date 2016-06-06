#include "default_worker.hpp"
#include "protocol.hpp"
#include "comm.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

DefaultWorker::DefaultWorker(std::unique_ptr<Comm> p_comm):
    comm(std::move(p_comm)),
    log(comm->get_addr()),
    tasks(log, *comm),
    should_stop(false)
{}

DefaultWorker::~DefaultWorker() {
    set_stopped(true);
}

void DefaultWorker::shutdown() {
    auto& remotes = comm->remote_endpoints();
    for (auto& r: remotes) {
        add_task(r, [] () { cur_worker->set_stopped(true); });
    }
    set_stopped(true);
}

void DefaultWorker::set_stopped(bool val) {
    should_stop = val;
}

void DefaultWorker::add_task(TaskT t) {
    tasks.add_task(std::move(t));
}

void DefaultWorker::add_task(const Address& where, TaskT t) {
    tasks.add_task(where, std::move(t));
}

size_t DefaultWorker::n_workers() const {
    return comm->remote_endpoints().size() + 1;
}

const Address& DefaultWorker::get_addr() const {
    return comm->get_addr();
}

bool DefaultWorker::is_stopped() const {
    return should_stop;
}

Comm& DefaultWorker::get_comm() {
    return *comm;
}

void DefaultWorker::recv() {
    comm->recv();
}

void DefaultWorker::one_step() {
    auto t = comm->recv();
    if (t != nullptr) {
        tasks.add_local_task(std::move(t));
    }

    if (tasks.size() == 0) {
        tasks.steal();
    } else {
        tasks.run_next();
    }
}

void DefaultWorker::run() {
    while (!is_stopped()) {
        one_step();
    }
}

void DefaultWorker::set_core_affinity(int core_id) {
    this->core_id = core_id;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(core_id, &cs);
    auto err = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    (void)err;
    tlassert(err == 0);
}

} //end namespace taskloaf

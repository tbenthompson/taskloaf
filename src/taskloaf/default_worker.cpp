#include "default_worker.hpp"
#include "protocol.hpp"
#include "comm.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

DefaultWorker::DefaultWorker(std::unique_ptr<Comm> p_comm):
    comm(std::move(p_comm)),
    tasks(*comm),
    ivar_tracker(*comm),
    should_stop(false)
{
    comm->add_handler(Protocol::Shutdown, [&] (Data) {
        stop();
    });
}

DefaultWorker::~DefaultWorker() {
    stop();
}

void DefaultWorker::shutdown() {
    auto& remotes = comm->remote_endpoints();

    for (auto& r: remotes) {
        comm->send(r, Msg(Protocol::Shutdown, make_data(10)));
    }
    stop();
}

void DefaultWorker::stop() {
    should_stop = true;
}

IVarTracker& DefaultWorker::get_ivar_tracker() {
    return ivar_tracker;
}

size_t DefaultWorker::n_workers() const {
    return comm->remote_endpoints().size() + 1;
}

bool DefaultWorker::can_compute_immediately() {
    if (immediate_computes > immediates_allowed) {
        immediate_computes = 0;
        if (tasks.size() > 0) {
            return false;
        }
        if (comm->remote_endpoints().size() > 0 && comm->has_incoming()) {
            return false;
        }
    }
    immediate_computes++;
    return true;
}

bool DefaultWorker::is_stopped() const {
    return should_stop;
}

Comm& DefaultWorker::get_comm() {
    return *comm;
}

void DefaultWorker::introduce(Address addr) {
    ivar_tracker.introduce(addr);
}

const Address& DefaultWorker::get_addr() {
    return get_comm().get_addr();
}

void DefaultWorker::add_task(TaskT f) {
    tasks.add_task(std::move(f));
}

void DefaultWorker::recv() {
    comm->recv();
}

void DefaultWorker::one_step() {
    if (comm->has_incoming()) {
        comm->recv();
        return;
    }
    
    if (tasks.size() == 0) {
        tasks.steal();
    } else {
        tasks.run_next();
    }
}

void DefaultWorker::run() {
    cur_worker = this;
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

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
    ivar_tracker(*comm)
{
    comm->add_handler(Protocol::Shutdown, [&] (Data) {
        stop = true;
    });
}

DefaultWorker::~DefaultWorker() {
    cur_worker = this;
    stop = true;
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
    return stop;
}

Comm& DefaultWorker::get_comm() {
    return *comm;
}

void DefaultWorker::shutdown() {
    auto& remotes = comm->remote_endpoints();

    for (auto& r: remotes) {
        comm->send(r, Msg(Protocol::Shutdown, make_data(10)));
    }
    stop = true;
}

void DefaultWorker::introduce(Address addr) {
    ivar_tracker.introduce(addr);
}

const Address& DefaultWorker::get_addr() {
    return get_comm().get_addr();
}

void DefaultWorker::add_task(TaskT f, bool push) {
    tasks.add_task(std::move(f), push);
}

void DefaultWorker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    ivar_tracker.fulfill(iv, std::move(vals));
}

void DefaultWorker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    ivar_tracker.add_trigger(iv, std::move(trigger));
}

void DefaultWorker::dec_ref(const IVarRef& iv) {
    // If the worker is shutting down, there is no need to dec-ref on a 
    // IVarRef destructor call. In fact, if we don't stop here, there will
    // be an infinite recursion.
    if (stop) {
        return;
    }
    ivar_tracker.dec_ref(iv);
}

void DefaultWorker::recv() {
    comm->recv();
}

void DefaultWorker::one_step() {
    if (comm->has_incoming()) {
        comm->recv();
        return;
    }

    tasks.steal();
    if (tasks.size() > 0) {
        tasks.next()();
    }
}

void DefaultWorker::run() {
    cur_worker = this;
    while (!stop) {
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

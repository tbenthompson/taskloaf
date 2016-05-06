#include "worker.hpp"
#include "worker_internals.hpp"
#include "protocol.hpp"
#include "comm.hpp"
#include "timing.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

thread_local Worker* cur_worker;

Worker::Worker(std::unique_ptr<Comm> p_comm):
    p(new WorkerInternals(std::move(p_comm)))
{
    p->comm->add_handler(Protocol::Shutdown, [&] (Data) {
        p->stop = true;
    });
}

Worker::~Worker() {
    cur_worker = this;
    p->stop = true;
}

bool Worker::can_compute_immediately() const {
    if (!(false &&
        p->immediate_computes > p->immediates_before_comm) &&
        !(p->tasks.size() > 0 &&
        p->immediate_computes > p->immediates_before_tasks))
    {
        return true;
    }
    p->immediate_computes = 0;
    return false;
}

bool Worker::is_stopped() const {
    return p->stop;
}

Comm& Worker::get_comm() {
    return *p->comm;
}

void Worker::shutdown() {
    auto& remotes = p->comm->remote_endpoints();
    for (auto& r: remotes) {
        p->comm->send(r, Msg(Protocol::Shutdown, make_data(10)));
    }
    p->stop = true;
}

void Worker::introduce(Address addr) {
    p->ivar_tracker.introduce(addr);
}

const Address& Worker::get_addr() {
    return get_comm().get_addr();
}

void Worker::add_task(TaskT f) {
    p->tasks.add_task(std::move(f));
}

void Worker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    p->ivar_tracker.fulfill(iv, std::move(vals));
}

void Worker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    p->ivar_tracker.add_trigger(iv, std::move(trigger));
}

void Worker::dec_ref(const IVarRef& iv) {
    // If the worker is shutting down, there is no need to dec-ref on a 
    // IVarRef destructor call. In fact, if we don't stop here, there will
    // be an infinite recursion.
    if (p->stop) {
        return;
    }
    p->ivar_tracker.dec_ref(iv);
}

void Worker::recv() {
    p->comm->recv();
}

void Worker::one_step() {
    if (p->comm->has_incoming()) {
        p->comm->recv();
        return;
    }

    p->tasks.steal();
    if (p->tasks.size() > 0) {
        p->tasks.next()();
    }
}

void Worker::run() {
    cur_worker = this;
    while (!p->stop) {
        one_step();
    }
}

void Worker::set_core_affinity(int core_id) {
    this->p->core_id = core_id;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(core_id, &cs);
    auto err = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    (void)err;
    tlassert(err == 0);
}

} //end namespace taskloaf

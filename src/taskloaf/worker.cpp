#include "worker.hpp"
#include "protocol.hpp"
#include "comm.hpp"
#include "timing.hpp"

#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

thread_local Worker* cur_worker;

Worker::Worker(std::unique_ptr<Comm> p_comm):
    comm(std::move(p_comm)),
    tasks(*comm),
    ivar_tracker(*comm)
{
    comm->add_handler(Protocol::Shutdown, [&] (Data) {
        stop = true;
    });
}

Worker::~Worker() {
    cur_worker = this;
    stop = true;
}

void Worker::shutdown() {
    auto& remotes = comm->remote_endpoints();
    for (auto& r: remotes) {
        comm->send(r, Msg(Protocol::Shutdown, make_data(10)));
    }
    stop = true;
}

void Worker::introduce(Address addr) {
    ivar_tracker.introduce(addr);
}

const Address& Worker::get_addr() {
    return comm->get_addr();
}

void Worker::add_task(TaskT f) {
    tasks.add_task(std::move(f));
}

void Worker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    ivar_tracker.fulfill(iv, std::move(vals));
}

void Worker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    ivar_tracker.add_trigger(iv, std::move(trigger));
}

void Worker::dec_ref(const IVarRef& iv) {
    // If the worker is shutting down, there is no need to dec-ref on a 
    // IVarRef destructor call. In fact, if we don't stop here, there will
    // be an infinite recursion.
    if (stop) {
        return;
    }
    ivar_tracker.dec_ref(iv);
}

void Worker::recv() {
    comm->recv();
}

void Worker::one_step() {
    if (comm->has_incoming()) {
        comm->recv();
        return;
    }

    tasks.steal();
    if (tasks.size() > 0) {
        tasks.next()();
    }
}

void Worker::run() {
    cur_worker = this;
    while (!stop) {
        one_step();
    }
}

void Worker::set_core_affinity(int core_id) {
    this->core_id = core_id;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(core_id, &cs);
    auto err = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    (void)err;
    tlassert(err == 0);
}

} //end namespace taskloaf

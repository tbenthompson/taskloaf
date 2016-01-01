#include "worker.hpp"
#include "communicator.hpp"

//TODO: Remove
#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

thread_local Worker* cur_worker;

Worker::Worker():
    comm(std::make_unique<CAFCommunicator>()),
    core_id(-1),
    stop(false)
{}

Worker::Worker(Worker&&) = default;

Worker::~Worker() {
    cur_worker = this;
}

void Worker::shutdown() {
    comm->send_shutdown();
    stop = true;
}

void Worker::meet(Address addr) {
    comm->meet(addr);
}

Address Worker::get_addr() {
    return comm->get_addr();
}

void Worker::add_task(TaskT f) {
    tasks.add_task(std::move(f));
}

std::pair<IVarRef,bool> Worker::new_ivar(const ID& id) {
    return ivars.new_ivar(comm->get_addr(), id);
}

void Worker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    if (iv.owner != comm->get_addr()) {
        comm->send_fulfill(iv, std::move(vals));
        return;
    }
    ivars.fulfill(iv, std::move(vals));
}

void Worker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    if (iv.owner != comm->get_addr()) {
        comm->send_add_trigger(iv, std::move(trigger));
        return;
    }
    ivars.add_trigger(iv, std::move(trigger));
}

void Worker::inc_ref(const IVarRef& iv) {
    if (iv.owner != comm->get_addr()) {
        comm->send_inc_ref(iv);
        return;
    }
    ivars.inc_ref(iv);
}

void Worker::dec_ref(const IVarRef& iv) {
    if (iv.owner != comm->get_addr()) {
        comm->send_dec_ref(iv);
        return;
    }
    // If the worker is shutting down, there is no need to dec-ref on a 
    // IVarRef destructor call. In fact, if we don't stop here, there will
    // be an infinite recursion.
    if (stop) {
        return;
    }
    ivars.dec_ref(iv);
}

void Worker::run() {
    int n_tasks = 0;

    typedef std::chrono::high_resolution_clock::time_point Time;
    auto now = [] () { return std::chrono::high_resolution_clock::now(); };
    auto since = [] (Time from) { 
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - from
        ).count();
    };

    Time start = now();
    int idle = 0;

    cur_worker = this;
    while (!stop) {
        comm->steal(tasks.size());
        stop = stop || comm->handle_messages(ivars, tasks);
        if (tasks.size() > 0) {
            idle += since(start);
            tasks.next()();
            start = now();
            n_tasks++;
        }
    }
    std::stringstream buf;
    buf << "n(" << core_id << "): " << n_tasks << " idle: " << idle << std::endl;
    std::cout << buf.rdbuf();
}

void Worker::set_core_affinity(int core_id) {
    this->core_id = core_id;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(core_id, &cs);
    auto err = pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    (void)err;
    assert(err == 0);
}

} //end namespace taskloaf

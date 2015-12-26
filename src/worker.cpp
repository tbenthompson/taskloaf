#include "worker.hpp"
#include "communicator.hpp"

#include <iostream>

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

IVarRef Worker::new_ivar() {
    return ivars.new_ivar(comm->get_addr());
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
    //TODO: This is sketchy... when the Worker destructor is called, the
    //IVarRef destructors for any of the contained things is called and
    //there is a reference loop via the cur_worker global variable.
    if (stop) {
        return;
    }
    if (iv.owner != comm->get_addr()) {
        comm->send_dec_ref(iv);
        return;
    }
    ivars.dec_ref(iv);
}

void Worker::run() {
    int n_tasks = 0;
    cur_worker = this;
    while (!stop) {
        comm->steal(tasks.size());
        if (tasks.size() > 0) {
            // std::cout << "n(" << core_id << "): " << n_tasks 
            //           << " " << tasks.size() << std::endl;
            tasks.next()();
            n_tasks++;
        }
        stop = stop || comm->handle_messages(ivars, tasks);
    }
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

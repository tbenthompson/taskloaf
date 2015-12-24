#include "worker.hpp"
#include "communicator.hpp"

#include <iostream>

namespace taskloaf {

thread_local Worker* cur_worker;

Worker::Worker():
    comm(std::make_unique<CAFCommunicator>()),
    ivars()
{
    
}

Worker::~Worker() {}

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
    if (iv.owner != comm->get_addr()) {
        comm->send_dec_ref(iv);
        return;
    }
    ivars.dec_ref(iv);
}

void Worker::run_no_stealing() {
    cur_worker = this;
    while (!ivars.empty() || !tasks.empty()) {
        tasks.next()();
        comm->handle_messages(ivars, tasks);
    }
}

void Worker::run_stealing() {
    int n_tasks = 0;
    cur_worker = this;
    while (true) {
        if (tasks.empty()) {
            comm->steal();
        } else {
            tasks.next()();
            n_tasks++;
            if (n_tasks % 100000 == 0) {
                std::cout << "n(" << core_id << "): " << n_tasks 
                          << " " << tasks.size() << std::endl;
            }
        }
        comm->handle_messages(ivars, tasks);
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

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
    std::cout << comm->get_addr().hostname << ":" << comm->get_addr().port << std::endl;
    std::cout << iv.owner.hostname << ":" << iv.owner.port << std::endl;
    if (iv.owner != comm->get_addr()) {
        comm->send_dec_ref(iv);
        return;
    }
    std::cout << "SUCC" << std::endl;
    ivars.dec_ref(iv);
}

//TODO: Maybe have two run versions. One runs endlessly for clients, the
//other runs only until there are no more tasks on this particular Worker.
//In other words, a stealing worker and a non-stealing worker.
void Worker::run() {
    cur_worker = this;
    while (!tasks.empty()) {
        tasks.next()();
        comm->handle_messages(ivars, tasks);
    }
}

} //end namespace taskloaf

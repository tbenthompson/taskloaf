#include "worker.hpp"
#include "caf_comm.hpp"
#include "protocol.hpp"

//TODO: Remove
#include <iostream>
#include <sstream>
#include <chrono>

namespace taskloaf {

thread_local Worker* cur_worker;

Worker::Worker():
    comm(std::make_unique<CAFComm>()),
    tasks(*comm),
    ivar_tracker(*comm)
{
    comm->add_handler(Protocol::Shutdown, [&] (Data) {
        stop = true;
    });
}

Worker::Worker(Worker&&) = default;

Worker::~Worker() {
    cur_worker = this;
    stop = true;
}

void Worker::shutdown() {
    comm->send_all(Msg(Protocol::Shutdown, make_data(10)));
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

std::pair<IVarRef,bool> Worker::new_ivar(const ID& id) {
    return ivar_tracker.new_ivar(id);
}

void Worker::fulfill(const IVarRef& iv, std::vector<Data> vals) {
    ivar_tracker.fulfill(iv, std::move(vals));
}

void Worker::add_trigger(const IVarRef& iv, TriggerT trigger) {
    ivar_tracker.add_trigger(iv, std::move(trigger));
}

void Worker::inc_ref(const IVarRef& iv) {
    ivar_tracker.inc_ref(iv);
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
        recv();
        tasks.steal();
        if (tasks.size() > 0) {
            idle += since(start);
            tasks.next()();
            start = now();
            n_tasks++;
        }
    }

    idle += since(start);
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

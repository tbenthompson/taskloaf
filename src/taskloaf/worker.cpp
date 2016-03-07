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

void Worker::run() {
    struct Timer {
        typedef std::chrono::high_resolution_clock::time_point Time;
        Time t_start;
        int time_ms = 0;

        void start() {
            t_start = std::chrono::high_resolution_clock::now();
        }
        void stop() {
            time_ms += std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - t_start
            ).count();
        }
    };

    int n_tasks = 0;
    Timer t_comm;
    Timer t_not_tasks;
    Timer t_total;
    t_not_tasks.start();
    t_total.start();

    cur_worker = this;
    while (!stop) {
        while (comm->has_incoming()) {
            t_comm.start();
            comm->recv();
            t_comm.stop();
        }

        tasks.steal();
        if (tasks.size() > 0) {

            t_not_tasks.stop();
            tasks.next()();
            t_not_tasks.start();

            n_tasks++;
        }
    }
    t_not_tasks.stop();
    t_total.stop();
    auto t_idle = t_not_tasks.time_ms - t_comm.time_ms;
    auto t_tasks = t_total.time_ms - t_not_tasks.time_ms;

    for (int i = 0; i < 10000000; i++) {
        comm->recv();
    }
    std::stringstream buf;
    buf << "n(" << core_id << "): " << n_tasks
        << " comm: " << t_comm.time_ms 
        << " tasks: " << t_tasks
        << " idle: " << t_idle
        << " undeleted ivars " << ivar_tracker.n_owned() << std::endl;
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

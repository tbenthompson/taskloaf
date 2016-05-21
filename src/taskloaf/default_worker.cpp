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

void DefaultWorker::add_task(TaskT f) {
    tasks.add_task(std::move(f));
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
    run_a_task();
}

void DefaultWorker::run_a_task() {
    if (tasks.size() > 0) {
        tasks.next()();
    }
}

void DefaultWorker::new_thread(std::function<void()> start_task) {
    waiting_threads.push(std::make_unique<Thread>(
        [&, start_task = std::move(start_task)] () {
            if (start_task != nullptr) {
                start_task();
            }
            while (!stop && waiting_threads.size() == 0) {
                one_step();
            }
        }
    ));
}

void DefaultWorker::run(std::function<void()> start_task) {
    cur_worker = this;
    while (!stop) {
        if (waiting_threads.size() == 0) {
            new_thread(std::move(start_task));
        }
        cur_thread = std::move(waiting_threads.front());
        waiting_threads.pop();
        cur_thread->switch_in();
    }
}

/* Yield is tricky because there are two situations:
 * 1) We are inside a task with a delayed launch. This is great and easy.
 * 2) We are inside a task with an immediate launch. In this situations, there
 * is no straightforward way to delay the task. As a result, yield only responds
 * to the asyncd and thend contexts.
 */
void DefaultWorker::yield() {
    new_thread([&] { run_a_task(); });
    waiting_threads.push(std::move(cur_thread));
    waiting_threads.back()->switch_out();
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

#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
#include "ivar_tracker.hpp"
#include "worker.hpp"
#include "thread.hpp"

#include <memory>
#include <queue>

namespace taskloaf { 

struct Comm;
struct DefaultWorker: public Worker {
    std::unique_ptr<Comm> comm;
    std::queue<Thread::Ptr> waiting_threads;
    Thread::Ptr cur_thread;
    TaskCollection tasks;
    IVarTracker ivar_tracker;
    int core_id = -1;
    bool stealing = false;
    bool should_stop = false;
    int immediate_computes = 0;
    static const int immediates_allowed = 100;

    DefaultWorker(std::unique_ptr<Comm> comm);
    DefaultWorker(DefaultWorker&&) = default;
    ~DefaultWorker();

    void shutdown() override;
    bool can_compute_immediately() override;
    size_t n_workers() const override;
    void add_task(TaskT f) override;
    void yield() override;
    IVarTracker& get_ivar_tracker() override;

    void introduce(Address addr);
    bool is_stopped() const;
    Comm& get_comm();
    const Address& get_addr();

    void stop();
    void recv();
    void one_step();
    void run_a_task();
    void new_thread(std::function<void()> start_task);
    void run(std::function<void()> start_task);
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

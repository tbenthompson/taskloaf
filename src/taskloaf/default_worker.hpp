#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
#include "ivar_tracker.hpp"
#include "worker.hpp"

#include <memory>
#include <queue>
#include <atomic>

namespace taskloaf { 

struct Comm;
struct DefaultWorker: public Worker {
    std::unique_ptr<Comm> comm;
    TaskCollection tasks;
    IVarTracker ivar_tracker;
    int core_id = -1;
    bool stealing = false;
    std::atomic<bool> should_stop;
    int immediate_computes = 0;
    static const int immediates_allowed = 0;

    DefaultWorker(std::unique_ptr<Comm> comm);
    DefaultWorker(const DefaultWorker&) = delete;
    ~DefaultWorker();

    void shutdown() override;
    void stop() override;
    void run() override;
    bool can_compute_immediately() override;
    size_t n_workers() const override;
    void add_task(TaskT f) override;
    IVarTracker& get_ivar_tracker() override;

    void introduce(Address addr);
    bool is_stopped() const;
    Comm& get_comm();
    const Address& get_addr();

    void recv();
    void one_step();
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

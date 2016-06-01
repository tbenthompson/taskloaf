#pragma once
#include "address.hpp"
#include "global_ref.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
#include "ref_tracker.hpp"
#include "worker.hpp"
#include "logging.hpp"

#include <memory>
#include <atomic>

namespace taskloaf { 

struct Comm;
struct DefaultWorker: public Worker {
    std::unique_ptr<Comm> comm;
    Log log;
    TaskCollection tasks;
    IVarTracker ref_tracker;

    int core_id = -1;
    bool stealing = false;
    std::atomic<bool> should_stop;
    int immediate_computes = 0;
    static const int immediates_allowed = 2;

    DefaultWorker(std::unique_ptr<Comm> comm);
    DefaultWorker(const DefaultWorker&) = delete;
    ~DefaultWorker();

    void shutdown() override;
    void stop() override;
    void run() override;
    const Address& get_addr() const override;
    size_t n_workers() const override;
    TaskCollection& get_task_collection() override;
    IVarTracker& get_ref_tracker() override;

    void introduce(Address addr);
    bool is_stopped() const;
    Comm& get_comm();

    void recv();
    void one_step();
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

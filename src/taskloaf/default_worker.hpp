#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
#include "ivar_tracker.hpp"
#include "worker.hpp"

#include <memory>

namespace taskloaf { 

struct Comm;
struct DefaultWorker: public Worker {
    std::unique_ptr<Comm> comm;
    TaskCollection tasks;
    IVarTracker ivar_tracker;
    int core_id = -1;
    bool stealing = false;
    bool stop = false;
    int immediate_computes = 0;
    static const int immediates_allowed = 10000;

    DefaultWorker(std::unique_ptr<Comm> comm);
    DefaultWorker(DefaultWorker&&) = default;
    ~DefaultWorker();

    bool can_compute_immediately();

    bool is_stopped() const;
    Comm& get_comm();
    size_t n_workers() const;
    const Address& get_addr();

    void shutdown();
    void introduce(Address addr);
    void add_task(TaskT f, bool push);
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void dec_ref(const IVarRef& ivar);
    void recv();
    void one_step();
    void run();
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

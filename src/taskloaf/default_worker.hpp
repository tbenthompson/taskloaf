#pragma once
#include "address.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
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

    int core_id = -1;
    bool stealing = false;
    std::atomic<bool> should_stop;
    int immediate_computes = 0;
    static const int immediates_allowed = 2;

    DefaultWorker(std::unique_ptr<Comm> comm);
    DefaultWorker(const DefaultWorker&) = delete;
    ~DefaultWorker();

    void shutdown() override;
    void set_stopped(bool) override;
    void run() override;
    const Address& get_addr() const override;
    size_t n_workers() const override;
    void add_task(closure t) override;
    void add_task(const Address& where, closure t) override;

    bool is_stopped() const;
    Comm& get_comm();

    void recv();
    void one_step();
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

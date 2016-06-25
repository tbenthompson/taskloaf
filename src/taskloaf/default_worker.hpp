#pragma once
#include "address.hpp"
#include "closure.hpp"
#include "task_collection.hpp"
#include "worker.hpp"
#include "logging.hpp"

#include <memory>
#include <atomic>

namespace taskloaf { 

struct comm;
struct default_worker: public worker {
    std::unique_ptr<comm> my_comm;
    logger log;
    task_collection tasks;

    int core_id = -1;
    bool should_stop = false;

    default_worker(std::unique_ptr<comm> comm);
    default_worker(const default_worker&) = delete;
    ~default_worker();

    void shutdown() override;
    void set_stopped(bool) override;
    void run() override;

    const address& get_addr() const override;
    size_t n_workers() const override;
    void add_task(const address& where, closure t) override;

    bool is_stopped() const;
    comm& get_comm();

    void one_step();
    void set_core_affinity(int core_id);
};

} //end namespace taskloaf

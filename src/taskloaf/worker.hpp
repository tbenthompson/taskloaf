#pragma once
#include "address.hpp"

#include <memory>

#include <x86intrin.h>


namespace taskloaf { 

struct closure;
struct worker {
    virtual ~worker() {}

    virtual void shutdown() = 0;
    virtual void set_stopped(bool) = 0;
    virtual void run() = 0;

    virtual const address& get_addr() const = 0;
    virtual size_t n_workers() const = 0;

    virtual void add_task(closure t) = 0;
    virtual void add_task(const address& where, closure t) = 0;

    uint64_t now() {
        return 0;
    }
    uint64_t last_poll = now();

    constexpr static uint64_t us = 1000;
    constexpr static uint64_t ms = us * 1000;
    constexpr static uint64_t polling_interval = 50000 * ms;
    bool should_run_now() volatile {
        return true;
        // return now() - last_poll < polling_interval;
    }

};

extern __thread worker* cur_worker;
extern __thread address cur_addr;

void set_cur_worker(worker* w);
void clear_cur_worker();

} //end namespace taskloaf

#pragma once
#include "address.hpp"

#include <atomic>

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

    std::atomic<bool> needs_interrupt;
    bool get_needs_interrupt() {
        return needs_interrupt.load(std::memory_order_relaxed); 
    }
    void set_needs_interrupt(bool val) {
        needs_interrupt.store(val, std::memory_order_relaxed); 
    }
};

extern __thread worker* cur_worker;

} //end namespace taskloaf

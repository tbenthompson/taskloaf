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


    bool needs_interrupt = false;
    bool should_run_now() volatile { return true; }
};

extern __thread worker* cur_worker;
extern __thread address cur_addr;

void set_cur_worker(worker* w);
void clear_cur_worker();

} //end namespace taskloaf

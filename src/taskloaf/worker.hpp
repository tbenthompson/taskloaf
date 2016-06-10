#pragma once
#include "address.hpp"
#include "closure.hpp"

#include <memory>

namespace taskloaf { 

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual void set_stopped(bool) = 0;
    virtual void run() = 0;

    virtual const Address& get_addr() const = 0;
    virtual size_t n_workers() const = 0;

    virtual void add_task(closure t) = 0;
    virtual void add_task(const Address& where, closure t) = 0;
};

extern __thread Worker* cur_worker;
extern __thread Address cur_addr;

void set_cur_worker(Worker* w);
void clear_cur_worker();

} //end namespace taskloaf

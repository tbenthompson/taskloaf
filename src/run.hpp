#pragma once
#include "worker.hpp"
#include "future.hpp"

namespace taskloaf {

IVarRef run_helper(const FutureNode& data);

template <typename T>
void run(const Future<T>& fut, Worker& w) {
    cur_worker = &w;
    run_helper(*fut.data.get());
    w.run();
    cur_worker = nullptr;
}

}

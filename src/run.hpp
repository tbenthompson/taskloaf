#pragma once
#include "worker.hpp"
#include "future.hpp"

namespace taskloaf {

IVarRef run_helper(const FutureNode& data);

template <typename F>
void launch(int n_workers, F f) {
    auto t = async(f).unwrap();
    launch_helper(n_workers, t.data);
}

void launch_helper(int n_workers, std::shared_ptr<FutureNode> f);
int shutdown();

template <typename T>
void run(const Future<T>& fut, Worker& w) {
    cur_worker = &w;
    run_helper(*fut.data);
    w.run();
    cur_worker = nullptr;
}


}

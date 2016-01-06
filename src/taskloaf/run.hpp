#pragma once
#include "future.hpp"

namespace taskloaf {

void launch_helper(size_t n_workers, std::shared_ptr<FutureNode> f);
int shutdown();
void run_helper(FutureNode& data);

template <typename F>
void launch(int n_workers, F f) {
    auto t = async(f).unwrap();
    launch_helper(n_workers, t.data);
}

template <typename... Ts>
void run(const Future<Ts...>& fut) {
    run_helper(*fut.data);
}

}

#pragma once

#include "future.hpp"

namespace tsk = taskloaf;

inline tsk::Future<int> fib(int index) {
    if (index < 3) {
        return tsk::ready(1);
    } else {
        auto af = fib(index - 1);
        auto bf = fib(index - 2);
        return tsk::when_all(af, bf).then([] (int a, int b) { return a + b; });
    }
}

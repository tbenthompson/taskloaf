#pragma once

#include "future.hpp"

namespace tsk = taskloaf;

inline int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

inline tsk::Future<int> fib(int index, int grouping = 3) {
    if (index < grouping) {
        return tsk::async([=] () { return fib_serial(index); });
    } else {
        auto af = fib(index - 1, grouping);
        auto bf = fib(index - 2, grouping);
        return tsk::when_all(af, bf).then([] (int a, int b) { return a + b; });
    }
}

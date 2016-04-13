#pragma once
#include "taskloaf.hpp"

inline int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

inline taskloaf::Future<int> fib(int index, int grouping = 3) {
    if (index < grouping) {
        return taskloaf::async([=] () { return fib_serial(index); });
    } else {
        auto af = fib(index - 1, grouping);
        auto bf = fib(index - 2, grouping);
        return taskloaf::when_all(af, bf).then(
            [] (int a, int b) { return a + b; }
        );
    }
}

inline taskloaf::Future<int> fib_unrevealed(int index, int grouping = 3) {
    if (index < grouping) {
        return taskloaf::async([=] () { return fib_serial(index); });
    } else {
        return taskloaf::async([=] () {
            auto af = fib(index - 1, grouping);
            auto bf = fib(index - 2, grouping);
            return taskloaf::when_all(af, bf).then(
                [] (int a, int b) { return a + b; }
            );
        }).unwrap();
    }
}


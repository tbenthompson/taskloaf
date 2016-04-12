#include "catch.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/worker.hpp"
#include "taskloaf.hpp"
#include <functional>

using namespace taskloaf;

int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}


Future<int> fib(int index, int grouping = 3) {
    if (index < grouping) {
        return async([=] () { return fib_serial(index); });
    } else {
        auto af = fib(index - 1, grouping);
        auto bf = fib(index - 2, grouping);
        return when_all(af, bf).then(
            [] (int a, int b) { return a + b; }
        );
    }
}

TEST_CASE("Fib") {
    size_t n = 20;
    size_t grouping = 15;
    launch_local_singlethread(10, [&] () {
        auto fut = fib(n, grouping);
        return fut.then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            return shutdown();
        });
    });
}

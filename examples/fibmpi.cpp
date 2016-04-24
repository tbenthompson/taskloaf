#include "taskloaf.hpp"

namespace tl = taskloaf;

int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

tl::Future<int> fib(int index, int grouping = 3) {
    if (index < grouping) {
        return tl::async([=] () { return fib_serial(index); });
    } else {
        auto af = fib(index - 1, grouping);
        auto bf = fib(index - 2, grouping);
        return tl::when_all(af, bf).then(
            [] (int a, int b) { return a + b; }
        );
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int grouping = std::stoi(std::string(argv[2]));

    tl::launch_mpi([&] () {
        return fib(n, grouping).then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            return tl::shutdown();
        });
    });
}

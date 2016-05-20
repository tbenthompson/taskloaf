#include "taskloaf.hpp"

namespace tl = taskloaf;

int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

tl::Future<int> fib(int index) {
    if (index < 3) {
        return tl::async([] () { return 1; });
    } else {
        // return tl::when_both(fib(index - 1), fib(index - 2))
        //     .then([] (int a, int b) { return a + b; });
        return tl::async([=] () {
            return tl::when_all(
                fib(index - 1),
                fib(index - 2)
            ).then(
                [] (int a, int b) { return a + b; }
            );
        }).unwrap();
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));

    tl::launch_mpi([&] () {
        return fib(n).then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            return tl::shutdown();
        });
    });
}

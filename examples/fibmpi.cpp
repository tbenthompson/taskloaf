#include <mpi.h>

#include <functional>

#include "fib.hpp"

using namespace taskloaf;

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int grouping = std::stoi(std::string(argv[2]));

    launch_mpi([&] () {
        auto fut = fib(n, grouping);
        return fut.then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            return shutdown();
        });
    });
}

#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

auto fib(int index, int threshold) {
    if (index < threshold) {
        return tl::async([=] () { return fib_serial(index); });
    } else {
        return tl::async([=] () {
            return tl::when_all(
                fib(index - 1, threshold),
                fib(index - 2, threshold)
            ).then(std::plus<int>());
        }).unwrap();
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int threshold = std::stoi(std::string(argv[2]));
    int n_cores = std::stoi(std::string(argv[3]));
    tl::Config cfg;
    cfg.print_stats = true;
    auto ctx = tl::launch_local(n_cores, cfg);
    auto x = fib(n, threshold).get();
    std::cout << "fib(" + std::to_string(n) + ") = " << x << std::endl;
}

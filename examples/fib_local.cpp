#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

auto fib_thresholded(int index) {
    if (index < 35) {
        return tl::async([=] () { return fib_serial(index); });
    } else {
        return tl::async([=] () {
            return tl::when_all(
                fib_thresholded(index - 1),
                fib_thresholded(index - 2)
            ).then(std::plus<int>());
        }).unwrap();
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int n_cores = std::stoi(std::string(argv[2]));
    auto ctx = tl::launch_local(n_cores);
    auto x = fib_thresholded(n).get();
    std::cout << "fib(" + std::to_string(n) + ") = " << x << std::endl;
}

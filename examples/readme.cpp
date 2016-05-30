#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib(int index) {
    if (index < 3) {
        return tl::ready(1);
    } else {
        return tl::when_all(fib(index - 1), fib(index - 2)).then(std::plus<int>());
    }
}

int main() {
    int n_cores = 2;
    auto ctx = tl::launch_local(n_cores);
    auto x = fib(25).get();
    std::cout << "fib(25) = " << x << std::endl;
}

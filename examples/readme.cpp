#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib(int index) {
    if (index < 3) {
        return tl::ready(1);
    } else {
        return tl::async([=] () {
            return tl::when_all(
                fib(index - 1),
                fib(index - 2)
            ).then(std::plus<int>());
        }).unwrap();
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int n_cores = std::stoi(std::string(argv[2]));
    auto ctx = tl::launch_local(n_cores);
    auto x = fib(n).get();
    std::cout << "fib(" + std::to_string(n) + ") = " << x << std::endl;
}

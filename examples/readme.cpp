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

int main() {
    auto ctx = tl::launch_local(2);
    auto v = fib(30).get();
    std::cout << "fib(30) = " << v << std::endl;
}

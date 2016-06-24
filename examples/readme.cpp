#include "taskloaf.hpp"

namespace tl = taskloaf;

auto fib(int index) {
    if (index < 3) {
        return tl::ready(1);
    } else {
        return tl::task([=] { return fib(index - 1); }).unwrap().then(
            [] (tl::future<int> a, int x) {
                return a.then(std::plus<int>{}, x);
            },
            fib(index - 2)
        ).unwrap();
    }
}

int main() {
    auto ctx = tl::launch_local(2);
    auto v = fib(30).get();
    std::cout << "fib(30) = " << v << std::endl;
}

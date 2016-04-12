#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"

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

Future<int> fib_unrevealed(int index, int grouping = 3) {
    if (index < grouping) {
        return async([=] () { return fib_serial(index); });
    } else {
        return async([=] () {
            auto af = fib(index - 1, grouping);
            auto bf = fib(index - 2, grouping);
            return when_all(af, bf).then(
                [] (int a, int b) { return a + b; }
            );
        }).unwrap();
    }
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int grouping = std::stoi(std::string(argv[2]));
    int n_workers = std::stoi(std::string(argv[3]));

    TIC;
    std::cout << fib_serial(n) << std::endl;
    TOC("serial");

    TIC2;
    launch_local_serializing(n_workers, [&] () {
        auto fut = fib(n, grouping);
        TOC("submit");
        TIC2;
        return fut.then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            TOC("run");
            TIC2;
            return shutdown();
        });
    });
    TOC("Clean up");
    // TOC("parallel fib " + std::to_string(n_workers));
}

#include "fib.hpp"
#include "taskloaf/timing.hpp"

using namespace taskloaf;

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

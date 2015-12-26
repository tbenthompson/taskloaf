#include "taskloaf.hpp"
#include "timing.hpp"

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
        return when_all(af, bf).then([] (int a, int b) { return a + b; });
    }
}

int main() {
    TIC;
    fib(45, 30);
    TOC("START");
    for (int n_workers = 1; n_workers <= 4; n_workers++) {
        TIC2;
        launch(n_workers, [] () {
            return fib(45, 30).then([] (int x) {
                std::cout << x << std::endl;
                return shutdown();
            });
        });
        TOC("Parallel fib " + std::to_string(n_workers));
    }
}

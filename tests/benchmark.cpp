#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"

using namespace taskloaf;

void run_many_times(int n_cores, int n, void (*fnc)()) {
    auto ctx = launch_local(n_cores);
    Timer time;
    time.start();
    for (int i = 0; i < n; i++) {
        fnc();
    }
    time.stop();
    std::cout << time.get_time() << "us" << std::endl;
}

#define BENCH(n_cores, n, fnc_name)\
    std::cout << "Running " << n << " iterations of " #fnc_name << " took ";\
    run_many_times(n_cores, n, &fnc_name);

UntypedFuture fib(int idx) {
    if (idx < 3) {
        return ready(ensure_data(1));
    } else {
        return async(Closure([=] (Data&, Data&) {
            return ensure_data(fib(idx - 1).then(Closure(
                [=] (UntypedFuture& a, int x) {
                    return ensure_data(a.then(Closure(
                        [x] (Data&, int y) { return x + y; }
                    )));
                },
                fib(idx - 2)
            )));
        })).unwrap();
    }
}

void bench_untyped_fib() {
    auto ctx = launch_local(1);
    std::cout << int(fib(27).get()) << std::endl;
}

void bench_ivar() {
    IVar ivar;
}

void bench_allocation() {
    auto* ptr = new int[10];
    delete ptr;
}

void bench_make_data() {
    auto d = ensure_data(10);
}

void bench_make_bigger_data() {
    auto d = ensure_data(std::vector<double>{1,2,3,4,5,6,7,8,9,10});
}

int main() {
    // BENCH(1, 1, bench_untyped_fib);
    // BENCH(1, 50000000, bench_ivar);
    BENCH(1, 10000000, bench_make_data);
    BENCH(1, 10000000, bench_make_bigger_data);
    // BENCH(1, 100000000, bench_allocation);

    // BENCH(1000000, bench_closure);
    // BENCH(1000000, bench_typed_closure);
    // BENCH(1000000, bench_make_data);
    // BENCH(1000000, bench_typed_data);
    // BENCH(1000000, bench_typed_data_ptr);
    // BENCH(1000000, bench_allocation);
    // BENCH(1, bench_100mb_allocation);
}

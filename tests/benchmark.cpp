#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"


using namespace taskloaf;

void run_many_times(int n, void (*fnc)()) {
    Timer time;
    time.start();
    for (int i = 0; i < n; i++) {
        fnc();
    }
    time.stop();
    std::cout << time.get_time() << "us" << std::endl;
}

#define BENCH(n, fnc_name)\
    std::cout << "Running " << n << " iterations of " #fnc_name << " took ";\
    run_many_times(n, &fnc_name);

UntypedFuture fib(int idx) {
    if (idx < 3) {
        return ready({make_data(1)});
    } else {
        return async(AsyncTaskT(
            [=] () {
                return std::vector<Data>{make_data(
                    when_all({fib(idx - 1), fib(idx - 2)}).then(
                        [] (std::vector<Data>& d) {
                            return std::vector<Data>{
                                make_data(d[0].get_as<int>() + d[1].get_as<int>())
                            };
                        }
                    )
                )};
            }
        )).unwrap();
    }
}

void bench_untyped_fib() {
    auto ctx = launch_local(1);
    std::cout << fib(27).get()[0].get_as<int>() << std::endl;
}

int main() {
    BENCH(1, bench_untyped_fib);
    // BENCH(1000000, bench_closure);
    // BENCH(1000000, bench_typed_closure);
    // BENCH(1000000, bench_make_data);
    // BENCH(1000000, bench_typed_data);
    // BENCH(1000000, bench_typed_data_ptr);
    // BENCH(1000000, bench_allocation);
    // BENCH(1, bench_100mb_allocation);
}

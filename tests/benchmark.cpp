#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"
#include "taskloaf/sized_any.hpp"

#include <locale>
#include <string>
#include <iostream>


using namespace taskloaf;

static size_t sum = 0;
int run_many_times(int n_cores, int n, size_t(*fnc)(), int baseline = 0) {
    auto ctx = launch_local(n_cores);
    Timer time;
    time.start();
    sum = 0;
    for (int i = 0; i < n; i++) {
        sum += fnc();
    }
    time.stop();
    return time.get_time() - baseline;
}

#define BENCH(n_cores, n, fnc_name)\
    {\
        std::string benchmark_name(#fnc_name);\
        bool should_run = benchmark_name.find(cmd_line_arg) != std::string::npos \
            || cmd_line_arg == "all";\
        if (should_run) {\
            auto empty = [] () { return size_t(0); };\
            auto baseline = run_many_times(1, n, empty);\
            auto t = run_many_times(n_cores, n, &fnc_name, baseline);\
            double average = static_cast<double>(t) / static_cast<double>(n);\
            std::cout << n << " iterations of " #fnc_name\
                << " took " << t << "us and average time of "\
                << average * 1000 << "ns" << std::endl;\
        }\
    }

UntypedFuture fib(int idx) {
    if (idx < 3) {
        return ready(ensure_data(1));
    } else {
        return async(closure([=] (Data&, Data&) {
            return ensure_data(fib(idx - 1).then(closure(
                [=] (UntypedFuture& a, int x) {
                    return ensure_data(a.then(closure(
                        [x] (Data&, int y) { return x + y; }
                    )));
                },
                fib(idx - 2)
            )).unwrap());
        })).unwrap();
    }
}

size_t untyped_fib() {
    auto ctx = launch_local(1);
    std::cout << "Fib: " << int(fib(23).get()) << std::endl;
    return 0;
}

template <int NCores>
size_t taskloaf_startup() {
    auto ctx = launch_local(NCores);
    return 0;
}

size_t ivar_construct() {
    IVar ivar;
    return 0;
}

size_t raw_int() {
    int x = 10;
    return x * x / (sum + 1);
}

size_t make_any() {
    auto d = ensure_any(10);
    return d.get<int>() * d.get<int>() / (sum + 1);
}

size_t vector_copy() {
    std::vector<double> a1{1};
    auto a2 = a1;
    return a2[1];
}

size_t any_copy() {
    auto a1 = ensure_any(std::vector<double>{1});
    auto a2 = a1;
    return a2.get<std::vector<double>>()[1];
}

size_t make_data() {
    auto d = ensure_data(10);
    return d.get_as<int>();
}

size_t make_bigger_data() {
    auto d = ensure_data(std::vector<double>{1,2,3,4,5,6,7,8,9,10});
    return 0;
}

int main(int argc, char** argv) {
    std::cout.imbue(std::locale(""));

    std::string cmd_line_arg = "all";
    if (argc > 1) {
        cmd_line_arg = argv[1];
    }

    BENCH(1, 1, untyped_fib);
    BENCH(1, 2000, taskloaf_startup<1>);
    BENCH(1, 2000, taskloaf_startup<2>);
    BENCH(1, 2000, taskloaf_startup<4>);
    BENCH(1, 1000000, ivar_construct);
    BENCH(1, 1000000, raw_int);
    BENCH(1, 1000000, make_any);
    BENCH(1, 1000000, vector_copy);
    BENCH(1, 1000000, any_copy);
    BENCH(1, 1000000, make_data);
    BENCH(1, 1000000, make_bigger_data);
}

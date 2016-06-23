#include "taskloaf/timing.hpp"
#include "taskloaf/data.hpp"
#include "taskloaf/future.hpp"
#include "taskloaf/typed_future.hpp"
#include "taskloaf.hpp"

#include <locale>
#include <string>
#include <iostream>

#include <cereal/types/vector.hpp>

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

untyped_future fib(int idx) {
    if (idx < 3) {
        return ut_ready(1);
    } else {
        return ut_async([idx] (_, _) {
            return fib(idx - 1).then(closure(
                [] (untyped_future& a, int x) {
                    return a.then([x] (_, int y) { return x + y; });
                },
                fib(idx - 2)
            )).unwrap();
        }).unwrap();
    }
}

size_t fib() {
    config cfg;
    cfg.print_stats = true;
    auto ctx = launch_local(12, cfg);
    std::cout << "Fib: " << int(fib(33).get()) << std::endl;
    return 0;
}

future<int> typed_fib(int idx) {
    if (idx < 3) {
        return ready(1);
    } else {
        return async([=] { return typed_fib(idx - 1); }).unwrap().then(
            [] (future<int> a, int x) {
                return a.then([x] (int y) { return x + y; });
            },
            typed_fib(idx - 2)
        ).unwrap();
    }
}

size_t typed_fib() {
    config cfg;
    cfg.print_stats = true;
    auto ctx = launch_local(1,cfg);
    std::cout << int(typed_fib(44).get()) << std::endl; 
    return 0;
}

template <int NCores>
size_t taskloaf_startup() {
    auto ctx = launch_local(NCores);
    return 0;
}

size_t fut_construct() {
    untyped_future fut;
    return 0;
}

size_t raw_int_ops() {
    int x = 10;
    return x * x / (sum + 1);
}

size_t vector_copy() {
    std::vector<double> a1{1};
    auto a2 = a1;
    return a2[1];
}

size_t any_copy() {
    auto a1 = data(std::vector<double>{1});
    auto a2 = a1;
    return a2.get<std::vector<double>>()[1];
}

size_t make_data() {
    auto d = data(10);
    return d.get<int>();
}

size_t make_bigger_data() {
    auto d = data(std::vector<double>{1,2,3,4,5,6,7,8,9,10});
    return 0;
}

#include <boost/pool/pool.hpp>

size_t boost_pool() {
    static boost::pool<> pool(32); 
    void* ptr = pool.malloc();
    pool.free(ptr);
    return (intptr_t)ptr;
}

size_t raw_alloc() {
    auto* ptr = new uint8_t[32];
    delete[] ptr;
    return (intptr_t)ptr;
}

int main(int argc, char** argv) {
    std::cout.imbue(std::locale(""));

    std::string cmd_line_arg = "all";
    if (argc > 1) {
        cmd_line_arg = argv[1];
    }

    BENCH(1, 1, fib);
    BENCH(1, 1, typed_fib);
    BENCH(1, 2000, taskloaf_startup<1>);
    BENCH(1, 2000, taskloaf_startup<2>);
    BENCH(1, 2000, taskloaf_startup<4>);
    BENCH(1, 1000000, fut_construct);
    BENCH(1, 1000000, vector_copy);
    BENCH(1, 1000000, any_copy);
    BENCH(1, 1000000, make_data);
    BENCH(1, 1000000, make_bigger_data);

    BENCH(1, 1000000, boost_pool);
    BENCH(1, 1000000, raw_alloc);
}

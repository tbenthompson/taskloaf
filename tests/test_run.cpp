#include "catch.hpp"

#include "run.hpp"
#include "fib.hpp"

using namespace taskloaf;

TEST_CASE("ST Scheduler") {
    int x = 0;
    Scheduler s;
    s.add_task([&] () { x = 1; });
    s.run();
    REQUIRE(x == 1);
}

TEST_CASE("ST Future") {
    STF stf;
    int x = 0;
    stf.add_trigger([&] (std::vector<Data>& val) { x = val[0].get_as<int>(); });
    Data d{make_safe_void_ptr(10)};
    stf.fulfill({d});
    REQUIRE(x == 10);
}

TEST_CASE("ST Future add trigger after fulfill") {
    STF stf;
    int x = 0;
    Data d{make_safe_void_ptr(10)};
    stf.fulfill({d});
    stf.add_trigger([&] (std::vector<Data>& val) { x = val[0].get_as<int>(); });
    REQUIRE(x == 10);
}

TEST_CASE("Run ready then") {
    Scheduler s;
    auto out = ready(10).then([] (int x) {
        REQUIRE(x == 10);
        return 0;
    });
    run(out, s);
}

TEST_CASE("Run async") {
    Scheduler s;
    auto out = async([] () {
        return 20;
    }).then([] (int x) {
        REQUIRE(x == 20);   
        return 0;
    });
    run(out, s);
}

TEST_CASE("Run when all") {
    Scheduler s;
    auto out = when_all(
        ready(10), ready(20)
    ).then([] (int x, int y) {
        return x * y; 
    }).then([] (int z) {
        REQUIRE(z == 200);
        return 0;
    });
    run(out, s);
}

TEST_CASE("Run unwrap") {
    Scheduler s;
    auto out = ready(10).then([] (int x) {
        if (x < 20) {
            return ready(5);
        } else {
            return ready(1);
        }
    }).unwrap().then([] (int y) {
        REQUIRE(y == 5);
        return 0;
    });
    run(out, s);
}

#include <chrono>

#define TIC\
    std::chrono::high_resolution_clock::time_point start =\
        std::chrono::high_resolution_clock::now();\
    int time_ms;
#define TIC2\
    start = std::chrono::high_resolution_clock::now();
#define TOC(name)\
    time_ms = std::chrono::duration_cast<std::chrono::milliseconds>\
                (std::chrono::high_resolution_clock::now() - start).count();\
    std::cout << name << " took "\
              << time_ms\
              << "ms.\n";

auto runner() {
    Scheduler s;
    TIC 
    auto task = fib(27).then([] (int x) { 
        // REQUIRE(x == 28657);
        std::cout << x << std::endl;
        return 0;
    });
    TOC("make task");
    TIC2
    run_helper(*task.data.get(), &s);
    TOC("plan");
    TIC2
    s.run();
    TOC("run");
    TIC2
    return start;
}

TEST_CASE("Run Fib") {
    auto start = runner();
    int time_ms;
    TOC("destruction")
//     run(task, s);
}

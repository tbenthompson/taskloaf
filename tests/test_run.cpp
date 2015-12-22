#include "catch.hpp"

#include "run.hpp"
#include "fib.hpp"

using namespace taskloaf;

TEST_CASE("Worker") {
    Worker w;
    int x = 0;
    w.add_task([&] () { x = 1; });
    w.run();
    REQUIRE(x == 1);
}

TEST_CASE("IVar trigger before fulfill") {
    Worker w;
    cur_worker = &w;
    auto ivar = w.new_ivar();
    int x = 0;
    w.add_trigger(ivar, [&] (std::vector<Data>& val) {
        x = val[0].get_as<int>(); 
    });
    Data d{make_safe_void_ptr(10)};
    w.fulfill(ivar, {d});
    REQUIRE(x == 10);
}

TEST_CASE("Trigger after fulfill") {
    Worker w;
    cur_worker = &w;
    auto ivar = w.new_ivar();
    int x = 0;
    Data d{make_safe_void_ptr(10)};
    w.fulfill(ivar, {d});
    w.add_trigger(ivar, [&] (std::vector<Data>& val) {
        x = val[0].get_as<int>(); 
    });
    REQUIRE(x == 10);
}

TEST_CASE("Run ready then") {
    Worker s;
    auto out = ready(10).then([] (int x) {
        REQUIRE(x == 10);
        return 0;
    });
    run(out, s);
}

TEST_CASE("Run async") {
    Worker s;
    auto out = async([] () {
        return 20;
    }).then([] (int x) {
        REQUIRE(x == 20);   
        return 0;
    });
    run(out, s);
}

TEST_CASE("Run when all") {
    Worker s;
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
    Worker s;
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
    TIC 
    auto task = fib(31).then([] (int x) { 
        // REQUIRE(x == 28657);
        std::cout << x << std::endl;
        return 0;
    });
    TOC("make task");
    TIC2
    Worker s;
    cur_worker = &s;
    run_helper(*task.data.get());
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

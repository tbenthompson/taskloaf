#include "catch.hpp"

#include "run.hpp"
#include "fib.hpp"

using namespace taskloaf;

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
    auto task = fib(21).then([] (int x) { 
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

//Took ~4 seconds for index = 31 on Tuesday 22 Dec.
TEST_CASE("Run Fib") {
    auto start = runner();
    int time_ms;
    TOC("destruction")
//     run(task, s);
}

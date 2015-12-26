#include "catch.hpp"

#include "taskloaf/run.hpp"

#include <thread>

using namespace taskloaf;

TEST_CASE("Run ready then") {
    auto out = ready(10).then([] (int x) {
        REQUIRE(x == 10);
        return shutdown();
    });
    run(out);
}

TEST_CASE("Run async") {
    auto out = async([] () {
        return 20;
    }).then([] (int x) {
        REQUIRE(x == 20);   
        return shutdown();
    });
    run(out);
}

TEST_CASE("Run when all") {
    auto out = when_all(
        ready(10), ready(20)
    ).then([] (int x, int y) {
        return x * y; 
    }).then([] (int z) {
        REQUIRE(z == 200);
        return shutdown();
    });
    run(out);
}

TEST_CASE("Run unwrap") {
    auto out = ready(10).then([] (int x) {
        if (x < 20) {
            return ready(5);
        } else {
            return ready(1);
        }
    }).unwrap().then([] (int y) {
        REQUIRE(y == 5);
        return shutdown();
    });
    run(out);
}

#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf; 

TEST_CASE("Run tree once", "[future]") {
    int x = 0;
    launch_local(1, [&] () {
        auto once_task = async([&] () { 
            x++;
            return 0; 
        });
        return when_all(
            once_task, once_task, once_task
        ).then([] (int, int, int) {
            return shutdown();
        });
    });
    REQUIRE(x == 1);
}

TEST_CASE("Empty futures", "[future]") {
    launch_local(1, [&] () {
        auto f = async([] () {})
            .then([] () { return 0; })
            .then([] (int) {})
            .then([] () {})
            .then([] () { return shutdown(); });
        return f;
    });
}

TEST_CASE("Outside of launch, just run immediately", "[future]") {
    int x = 0;
    when_all(async([&] { x = 1; })
            .then([&] {x *= 2; return 1; })
        ).then([] (int) { return ready(1); })
        .unwrap()
        .then([&] (int y) { x += y; });
    REQUIRE(x == 3);
}

TEST_CASE("Empty when_all", "[future]") {
    int x = 0;
    launch_local(1, [&] () {
        auto f = when_all(async([&] {
                x = 2;
            }), ready(2))
            .then([&] (int mult) {
                x *= mult;
            });
        return ready(shutdown());
    });
    REQUIRE(x == 4);
}

#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf; 

TEST_CASE("Empty futures", "[empty_future]") {
    launch_local(1, [&] () {
        auto f = async([] () {})
            .then([] () { return 0; })
            .then([] (int) {})
            .then([] () {})
            .then([] () { return shutdown(); });
        return f;
    });
}

TEST_CASE("Empty delayed futures", "[empty_future]") {
    launch_local(1, [&] () {
        auto f = asyncd([] () {})
            .thend([] () { return 0; })
            .thend([] (int x) { REQUIRE(x == 0); })
            .thend([] () {})
            .thend([] () { return shutdown(); });
        return f;
    });
}

TEST_CASE("Outside of launch, just run immediately", "[empty_future]") {
    int x = 0;
    when_all(
            async([&] { x = 1; }).then([&] {x *= 2; }),
            ready(5)
        ).then([] (int) { return ready(1); })
        .unwrap()
        .then([&] (int y) { x += y; });
    REQUIRE(x == 3);
}

TEST_CASE("Empty when_all", "[empty_future]") {
    int x = 0;
    when_all(async([&] { x = 2; }), ready(2))
        .then([&] (int mult) { x *= mult; });
    REQUIRE(x == 4);
}

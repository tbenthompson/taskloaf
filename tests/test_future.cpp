#include "catch.hpp"

#include "taskloaf.hpp"

using namespace taskloaf; 

TEST_CASE("Ready") {
    auto ctx = launch_local(2);
    REQUIRE(ready(1).get() == 1);
}

TEST_CASE("Then") {
    auto ctx = launch_local(2);
    REQUIRE(ready(0).then([] (int x) { return x + 1; }).get() == 1);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(2);
    auto x = ready(4).then([] (int x) {
        if (x < 5) {
            return ready(1);
        } else {
            return ready(0);
        }
    }).unwrap().get();
    REQUIRE(x == 1);
}

TEST_CASE("When all") {
    auto ctx = launch_local(2);
    auto x = when_all(ready(0.2), ready(5)).then(
        [] (double a, int b) -> int {
            return a * b;
        }).get();
    REQUIRE(x == 1);
}

TEST_CASE("Async") {
    auto ctx = launch_local(2);
    REQUIRE(async([] () { return 1; }).get() == 1);
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc") {
    auto ctx = launch_local(2);
    async(bark).wait();
}

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc") {
    auto ctx = launch_local(2);
    ready(2).then(make_sound).wait();
}

void bark_silently() {}
TEST_CASE("void async fnc") {
    auto ctx = launch_local(2);
    async(bark_silently).wait();
}

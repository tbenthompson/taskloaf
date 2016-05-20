#include "catch.hpp"

#include "taskloaf/thread.hpp"

using namespace taskloaf;

TEST_CASE("Thread") {
    int x = 0;
    Thread t([&] () {
        x = 1; 
    });
    t.switch_in();
    REQUIRE(x == 1);
}

TEST_CASE("Switch out") {
    int x = 0;
    Thread t([&] () {
        t.switch_out();
        x = 1; 
    });
    t.switch_in();
    REQUIRE(x == 0);
}

TEST_CASE("Switch back in") {
    int x = 0;
    Thread t([&] () {
        t.switch_out();
        x = 1; 
    });
    t.switch_in();
    t.switch_in();
    REQUIRE(x == 1);
}

TEST_CASE("Bounce between threads") {
    int x = 0;
    Thread t([&] () {
        REQUIRE(x == 0);
        x = 1; 
    });
    Thread t2([&] () {
        t.switch_in();
        REQUIRE(x == 1);
    });
    t2.switch_in();
}

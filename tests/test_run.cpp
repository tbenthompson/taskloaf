#include "catch.hpp"

#include "run.hpp"

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
    stf.add_trigger([&] (Data& val) { x = val.get_as<int>(); });
    Data d{make_safe_void_ptr(10)};
    stf.fulfill(d);
    REQUIRE(x == 10);
}

TEST_CASE("ST Future add trigger after fulfill") {
    STF stf;
    int x = 0;
    Data d{make_safe_void_ptr(10)};
    stf.fulfill(d);
    stf.add_trigger([&] (Data& val) { x = val.get_as<int>(); });
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

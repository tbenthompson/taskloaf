#include "catch.hpp"

#include "serialize.hpp"
#include "taskloaf.hpp"

using namespace taskloaf;

TEST_CASE("Copying promotes", "[promotion]") {
    launch_local(1, [&] () {
        auto fut = async([] {});
        REQUIRE(!fut.is_global());
        auto fut2 = fut;
        REQUIRE(fut2.is_global());
        shutdown();
    });
}

TEST_CASE("Serializing promotes", "[promotion]") {
    launch_local(1, [&] () {
        auto fut = async([] {});
        auto s = serialize(fut);
        REQUIRE(fut.is_global());
        shutdown();
    });
}

TEST_CASE("Local from fulfilled global", "[promotion]") {
    launch_local(1, [&] () {
        auto fut = async([] { return 1; });
        auto fut2 = fut;
        auto fut3 = fut2.then([] (int x) { REQUIRE(x == 1); });
        REQUIRE(!fut3.is_global());
        shutdown();
    });
}

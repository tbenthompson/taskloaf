#include "catch.hpp"

#include "serialize.hpp"
#include "taskloaf.hpp"

using namespace taskloaf;

TEST_CASE("Copying promotes", "[promotion]") {
    auto ctx = launch_local(1);
    auto fut = async([] {});
    REQUIRE(!fut.is_global());
    auto fut2 = fut;
    REQUIRE(fut2.is_global());
}

TEST_CASE("Serializing promotes", "[promotion]") {
    auto ctx = launch_local(1);
    auto fut = async([] {});
    auto s = serialize(fut);
    REQUIRE(fut.is_global());
}

TEST_CASE("Local from fulfilled global", "[promotion]") {
    auto ctx = launch_local(1);
    auto fut = async([] { return 1; });
    auto fut2 = fut;
    auto fut3 = fut2.then([] (int x) { REQUIRE(x == 1); });
    REQUIRE(!fut3.is_global());

    auto fut4 = when_all(fut2, fut2).then([] (int x, int y) {
        REQUIRE(x == 1);
        REQUIRE(y == 1);
    });
    REQUIRE(!fut4.is_global());

    auto fut5 = async([] { return async([] { return 1; }); });
    auto fut6 = fut5;
    auto fut7 = fut6.unwrap().then([] (int x) { REQUIRE(x == 1); });
    REQUIRE(!fut7.is_global());
}

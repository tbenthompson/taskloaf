#include "catch.hpp"

#include "fnc.hpp"

TEST_CASE("Functor by type") {
    auto f = [] () { return 10; };
    CallFunctorByType<decltype(f)> caller;
    REQUIRE(caller() == 10);
}

TEST_CASE("Check registered") {
    REQUIRE(fnc_registry.size() == 2);
    auto name = make_fnc([] () { return 10; });
    make_fnc([] () { return 11; });
    REQUIRE(fnc_registry.count(name) == 1);
}

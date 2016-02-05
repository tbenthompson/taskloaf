#include "catch.hpp"

#include "taskloaf/ivar.hpp"
#include <iostream>

using namespace taskloaf;

TEST_CASE("Initialize", "[ref]") {
    ReferenceCount rc;
    REQUIRE(rc.cs.size() == 1);
    REQUIRE(rc.cs[0] == 1);
}

TEST_CASE("Decrement initial reference", "[ref]") {
    ReferenceCount rc;
    REQUIRE(rc.alive());
    rc.dec({{0,0},0,0});
    REQUIRE(!rc.alive());
}

TEST_CASE("Copy then decrement", "[ref]") {
    ReferenceCount rc;
    RefData ref{{0,0},0,0};
    auto ref2 = copy_ref(ref);

    SECTION("Child first") {
        rc.dec(ref2);
        REQUIRE(rc.alive());
        rc.dec(ref);
        REQUIRE(!rc.alive());
    }
    
    SECTION("Parent first") {
        rc.dec(ref);
        REQUIRE(rc.alive());
        rc.dec(ref2);
        REQUIRE(!rc.alive());
    }
}

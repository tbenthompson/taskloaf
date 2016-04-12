#include "catch.hpp"

#include "taskloaf/ref_counting.hpp"
#include <iostream>

using namespace taskloaf;

TEST_CASE("Initialize", "[ref]") {
    ReferenceCount rc;
    REQUIRE(rc.counts[0] == 0);
    REQUIRE(rc.deletes.size() == 0);
    REQUIRE(rc.alive());
}

TEST_CASE("Decrement initial reference", "[ref]") {
    ReferenceCount rc;
    rc.dec({0,0,0});
    REQUIRE(rc.dead());
}

TEST_CASE("Copy then decrement", "[ref]") {
    ReferenceCount rc;
    RefData ref{0,0,0};
    auto ref2 = copy_ref(ref);

    SECTION("Child first") {
        rc.dec(ref2);
        REQUIRE(rc.alive());
        rc.dec(ref);
        REQUIRE(rc.dead());
    }
    
    SECTION("Parent first") {
        rc.dec(ref);
        REQUIRE(rc.alive());
        rc.dec(ref2);
        REQUIRE(rc.dead());
    }
}

TEST_CASE("Merge no children", "[ref]") {
    ReferenceCount rc1;
    ReferenceCount rc2;
    rc2.merge(rc1);
    rc2.dec({0, 0, 0});
    rc2.dec({1, 0, 0});
    REQUIRE(rc2.dead());
}

TEST_CASE("Merge with children", "[ref]") {
    ReferenceCount rc1;
    rc1.dec({0, 0, 1});
    ReferenceCount rc2;
    rc2.dec({1, 1, 1});
    rc2.merge(rc1);
    rc2.dec({2, 2, 0});
    rc2.dec({3, 0, 0});
    REQUIRE(rc2.dead());
}

TEST_CASE("Merge with initial refs already dead", "[ref]") { 
    ReferenceCount rc1;
    rc1.dec({0, 0, 1});
    ReferenceCount rc2;
    rc2.dec({1, 0, 1});
    rc2.merge(rc1);
    rc2.dec({2, 1, 0});
    rc2.dec({3, 1, 0});
    REQUIRE(rc2.dead());
}

TEST_CASE("Source copy", "[ref]") {
    ReferenceCount rc1;
    auto a = copy_ref(rc1.source_ref);
    rc1.dec(a);
    rc1.dec({0, 0, 0});
    REQUIRE(rc1.dead());
}

TEST_CASE("Merge with source copy", "[ref]") {
    ReferenceCount rc1;
    auto src_copy = copy_ref(rc1.source_ref);

    ReferenceCount rc2;
    rc2.merge(rc1);

    SECTION("Delete src_copy first") {
        REQUIRE(rc2.alive());
        rc2.dec(src_copy);
        REQUIRE(rc2.alive());
        rc2.dec({0, 0, 0});
        rc2.dec({1, 0, 0});
        REQUIRE(rc2.dead());
    }

    SECTION("Delete src_copy second") {
        REQUIRE(rc2.alive());
        rc2.dec({0, 0, 0});
        rc2.dec({1, 0, 0});
        REQUIRE(rc2.alive());
        rc2.dec(src_copy);
        REQUIRE(rc2.dead());
    }
}

TEST_CASE("Merge twice", "[ref]") {
    ReferenceCount rc1;
    auto a = copy_ref(rc1.source_ref);

    ReferenceCount rc2;
    rc2.merge(rc1);

    ReferenceCount rc3;
    rc3.merge(rc2);
    rc3.dec({0, 0, 0});
    rc3.dec({1, 0, 0});
    rc3.dec({2, 0, 0});
    rc3.dec(a);
    REQUIRE(rc3.dead());
}

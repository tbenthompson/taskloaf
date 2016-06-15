#include "catch.hpp"

#include "taskloaf/data.hpp"
#include "taskloaf/closure.hpp"
#include "ownership_tracker.hpp"
#include "serialize.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace taskloaf;

TEST_CASE("Create trivial") {
    auto a = data(10); 
    REQUIRE(a.get<int>() == 10);
}

TEST_CASE("Create vector") {
    auto a = data(std::vector<double>{1,2,3}); 
    REQUIRE(!a.empty());
    auto vec = a.get<std::vector<double>>();
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[2] == 3);
}

TEST_CASE("Constructors called") {
    OwnershipTracker::reset();

    SECTION("Move construct") {
        auto d3 = data(OwnershipTracker());
        REQUIRE(OwnershipTracker::moves() == 1);
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Copy construct") {
        OwnershipTracker ot;
        auto d3 = data(ot);
        REQUIRE(OwnershipTracker::moves() == 0);
        REQUIRE(OwnershipTracker::copies() == 1);
    }
}

TEST_CASE("Assignment operator") {
    data d;
    auto d2 = data(OwnershipTracker());
    OwnershipTracker::reset();

    d = std::move(d2);
    // Just moves the pointer!
    REQUIRE(OwnershipTracker::moves() == 0);
    REQUIRE(OwnershipTracker::copies() == 0);
}

TEST_CASE("Destructors called") {
    SECTION("Destructor") {
        {
            auto d = data(OwnershipTracker());
            OwnershipTracker::reset();
        }
        REQUIRE(OwnershipTracker::deletes() == 1);
    }

    SECTION("Move assign destructs") {
        auto d = data(OwnershipTracker());
        OwnershipTracker::reset();
        d = data{};
        REQUIRE(OwnershipTracker::deletes() == 1);
    }
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = data(10);
    auto s = serialize(d);
    auto d2 = deserialize<decltype(d)>(s);
    REQUIRE(d2.get<int>() == 10);
}

TEST_CASE("Measure serialized size", "[data]") {
    SECTION("string") {
        std::string s("abcdef");
        auto d = data(s);
        REQUIRE(serialize(d).size() == 30);
    }

    SECTION("double") {
        auto d = data(0.015);
        REQUIRE(serialize(d).size() == 24);
    }
}

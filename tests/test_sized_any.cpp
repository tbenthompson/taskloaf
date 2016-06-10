#include "catch.hpp"

#include "taskloaf/sized_any.hpp"
#include "ownership_tracker.hpp"
#include "serialize.hpp"

#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/string.hpp>

#include <array>

using namespace taskloaf;

struct Empty {
    Empty() = default;
    Empty(const Empty&) {}
};

TEST_CASE("Ranged ensure_any") {
    REQUIRE(ensure_any(float(10)).max_size == 4);
    REQUIRE(ensure_any(double(10)).max_size == 8);
    REQUIRE(ensure_any(std::tuple<double,long>()).max_size == 16);
    REQUIRE(ensure_any(std::vector<double>()).max_size == 24);
    REQUIRE(ensure_any(std::array<double,16>()).max_size == 128);
    REQUIRE(ensure_any(std::array<double,17>()).max_size == 136);
    REQUIRE(ensure_any(std::array<double,100>()).max_size == 800);
}

TEST_CASE("Check standard layout") {
    static_assert(std::is_standard_layout<sized_any<4,false>>::value,
        "sized_any needs to be standard layout");
    static_assert(std::is_standard_layout<sized_any<4,true>>::value,
        "sized_any needs to be standard layout");
}

TEST_CASE("Create trivial") {
    auto a = ensure_any(10); 
    REQUIRE(a.get<int>() == 10);
}

TEST_CASE("Ensure any with any") {
    auto a = ensure_any(ensure_any(std::vector<double>{2.1}));
    REQUIRE(a.get<std::vector<double>>()[0] == 2.1);
}

TEST_CASE("Create vector") {
    auto a = ensure_any(std::vector<double>{1,2,3}); 
    REQUIRE(!a.empty());
    auto vec = a.get<std::vector<double>>();
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[2] == 3);
}

TEST_CASE("Constructors called") {
    OwnershipTracker::reset();

    SECTION("Move construct") {
        auto d3 = ensure_any(OwnershipTracker());
        REQUIRE(OwnershipTracker::moves() == 1);
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Copy construct") {
        OwnershipTracker ot;
        auto d3 = ensure_any(ot);
        REQUIRE(OwnershipTracker::moves() == 0);
        REQUIRE(OwnershipTracker::copies() == 1);
    }
}

TEST_CASE("Assignment constructors called") {
    auto d = ensure_any(Empty());
    auto d2 = ensure_any(OwnershipTracker());
    OwnershipTracker::reset();

    SECTION("Move assign") {
        d = std::move(d2);
        REQUIRE(OwnershipTracker::moves() == 1);
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Copy assign") {
        d = d2;
        REQUIRE(OwnershipTracker::moves() == 0);
        REQUIRE(OwnershipTracker::copies() == 1);
    }
}

TEST_CASE("Destructors called") {
    SECTION("Destructor") {
        {
            auto d = ensure_any(OwnershipTracker());
            OwnershipTracker::reset();
        }
        REQUIRE(OwnershipTracker::deletes() == 1);
    }

    SECTION("Move assign destructs") {
        auto d = ensure_any(OwnershipTracker());
        OwnershipTracker::reset();
        d = ensure_any(Empty());
        REQUIRE(OwnershipTracker::deletes() == 1);
    }

    SECTION("Copy assign destructs") {
        auto d = ensure_any(OwnershipTracker());
        auto d2 = ensure_any(Empty());
        OwnershipTracker::reset();
        d = d2;
        REQUIRE(OwnershipTracker::deletes() == 1);
    }
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = ensure_any(10);
    auto s = serialize(d);
    auto d2 = deserialize<decltype(d)>(s);
    REQUIRE(d2.get<int>() == 10);
}

TEST_CASE("Measure serialized size", "[data]") {
    auto baseline_non_trivial = serialize(ensure_any(Empty())).size();
    auto baseline_trivial = serialize(ensure_any(false)).size() - 1;

    SECTION("string") {
        std::string s("abcdef");
        auto d = ensure_any(s);
        REQUIRE(serialize(d).size() - baseline_non_trivial == 14);
    }

    SECTION("double") {
        auto d = ensure_any(0.015);
        REQUIRE(serialize(d).size() - baseline_trivial == 8);
    }
}

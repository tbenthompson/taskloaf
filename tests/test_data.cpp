#include "catch.hpp"

#include "taskloaf/data.hpp"
#include "taskloaf/closure.hpp"

#include "serialize.hpp"
#include "serializable_functor.hpp"
#include "ownership_tracker.hpp"

#include <cereal/types/string.hpp>

using namespace taskloaf;

TEST_CASE("Typed Data") {
    auto d = ensure_data(10);
    int v = d;
    REQUIRE(v == 10);
}

TEST_CASE("make data", "[data]") {
    auto d = ensure_data(std::string("yea!"));
    REQUIRE(d.get_as<std::string>() == "yea!");
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = ensure_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data>(ss);
    REQUIRE(d2.get_as<int>() == 10);
}

TEST_CASE("Reserialize opened", "[data]") {
    auto d = ensure_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data>(ss);
    REQUIRE(d2.get_as<int>() == 10);

    auto ss2 = serialize(d2);
    auto d3 = deserialize<Data>(ss2);
    REQUIRE(d3.get_as<int>() == 10);
}

TEST_CASE("Reserialize unopened", "[data]") {
    auto d = ensure_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data>(ss);
    auto ss2 = serialize(d2);
    auto d3 = deserialize<Data>(ss2);
    REQUIRE(d3.get_as<int>() == 10);
}

TEST_CASE("Measure serialized size", "[data]") {
    auto baseline = serialize(ensure_data(false)).size() - 1;
    SECTION("string") {
        std::string s("abcdef");
        auto d = ensure_data(s);
        REQUIRE(serialize(d).size() - baseline == 14);
    }

    SECTION("double") {
        auto d = ensure_data(0.015);
        REQUIRE(serialize(d).size() - baseline == 8);
    }
}

TEST_CASE("Deleter called", "[data]") {
    OwnershipTracker::reset();
    OwnershipTracker a;
    {
        auto d = ensure_data(a);
    }
    REQUIRE(OwnershipTracker::deletes() == 1);
}
    
TEST_CASE("Deserialized deleter called", "[data]") {
    OwnershipTracker a;
    auto d = ensure_data(a);
    OwnershipTracker::deletes() = 0;
    {
        auto ss = serialize(d);
        auto d2 = deserialize<Data>(ss);
    }
    REQUIRE(OwnershipTracker::deletes() == 1);
}

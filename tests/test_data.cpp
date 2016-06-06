#include "catch.hpp"

#include "taskloaf/data.hpp"
#include "taskloaf/closure.hpp"

#include "serialize.hpp"
#include "serializable_functor.hpp"
#include "ownership_tracker.hpp"

using namespace taskloaf;

TEST_CASE("Typed Data") {
    Data<int> d(10);
    int v = d;
    REQUIRE(v == 10);
}

TEST_CASE("make data", "[data]") {
    auto d = make_data(std::string("yea!"));
    REQUIRE(d.get() == "yea!");
}

TEST_CASE("Copying promotes", "[data]") {
    Data<int> d = make_data(10);
    Data<int> d2 = d;
    d2.get() = 11;
    REQUIRE(d.get() == 11);
}

TEST_CASE("Move doesn't promote") {
    Data<int> d = make_data(10);
    Data<int> d2 = std::move(d);
    REQUIRE(d2.which == 0);
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data<int>>(ss);
    REQUIRE(d2.get() == 10);
}

TEST_CASE("Reserialize opened", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data<int>>(ss);
    REQUIRE(d2.get() == 10);

    auto ss2 = serialize(d2);
    auto d3 = deserialize<Data<int>>(ss2);
    REQUIRE(d3.get() == 10);
}

TEST_CASE("Reserialize unopened", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize<Data<int>>(ss);
    auto ss2 = serialize(d2);
    auto d3 = deserialize<Data<int>>(ss2);
    REQUIRE(d3.get() == 10);
}

TEST_CASE("Measure serialized size", "[data]") {
    auto baseline = serialize(make_data(false)).size() - 1;
    SECTION("string") {
        std::string s("abcdef");
        auto d = make_data(s);
        REQUIRE(serialize(d).size() - baseline == 14);
    }

    SECTION("double") {
        auto d = make_data(0.015);
        REQUIRE(serialize(d).size() - baseline == 8);
    }
}

TEST_CASE("Deleter called", "[data]") {
    OwnershipTracker::reset();
    OwnershipTracker a;
    {
        auto d = make_data(a);
    }
    REQUIRE(OwnershipTracker::deletes() == 1);
}
    
TEST_CASE("Deserialized deleter called", "[data]") {
    OwnershipTracker a;
    auto d = make_data(a);
    OwnershipTracker::deletes() = 0;
    {
        auto ss = serialize(d);
        auto d2 = deserialize<Data<OwnershipTracker>>(ss);
    }
    REQUIRE(OwnershipTracker::deletes() == 1);
}

TEST_CASE("Implicit conversion") {
    auto d = make_data(10);
    int a = d;
}

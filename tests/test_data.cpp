#include "catch.hpp"

#include "taskloaf/data.hpp"

#include "delete_tracker.hpp"

using namespace taskloaf;

int DeleteTracker::deletes = 0;

TEST_CASE("make data", "[data]") {
    auto d = make_data(std::string("yea!"));
    REQUIRE(d.get_as<std::string>() == "yea!");
}

std::string serialize(Data d) {
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(d);
    return ss.str();
}

Data deserialize(std::string s) {
    std::stringstream ss(s);
    cereal::BinaryInputArchive iarchive(ss);
    Data d2;
    iarchive(d2);
    return d2;
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize(ss);
    REQUIRE(d2.get_as<int>() == 10);
}

TEST_CASE("Reserialize opened", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize(ss);
    REQUIRE(d2.get_as<int>() == 10);

    auto ss2 = serialize(d2);
    auto d3 = deserialize(ss2);
    REQUIRE(d3.get_as<int>() == 10);
}

TEST_CASE("Reserialize unopened", "[data]") {
    auto d = make_data(10);
    auto ss = serialize(d);
    auto d2 = deserialize(ss);
    auto ss2 = serialize(d2);
    auto d3 = deserialize(ss2);
    REQUIRE(d3.get_as<int>() == 10);
}

TEST_CASE("Measure serialized size", "[data]") {
    auto baseline = serialize(make_data(false)).size() - 1;
    SECTION("string") {
        std::string s("abcdef");
        auto d = make_data(s);
        REQUIRE(serialize(d).size() - baseline == 14);
    }

    SECTION("int") {
        auto d = make_data(10);
        REQUIRE(serialize(d).size() - baseline == 4);
    }

    SECTION("double") {
        auto d = make_data(0.015);
        REQUIRE(serialize(d).size() - baseline == 8);
    }
}

TEST_CASE("Deleter called", "[data]") {
    DeleteTracker::deletes = 0;
    DeleteTracker a;
    {
        auto d = make_data(a);
    }
    REQUIRE(DeleteTracker::deletes == 1);
}
    
TEST_CASE("Deserialized deleter called", "[data]") {
    DeleteTracker a;
    auto d = make_data(a);
    DeleteTracker::deletes = 0;
    {
        auto ss = serialize(d);
        auto d2 = deserialize(ss);
    }
    REQUIRE(DeleteTracker::deletes == 1);
}

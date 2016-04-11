#include "catch.hpp"

#include "taskloaf/data.hpp"

using namespace taskloaf;

TEST_CASE("make data", "[data]") {
    auto d = make_data(std::string("yea!"));
    REQUIRE(d.get_as<std::string>() == "yea!");
}

std::stringstream serialize(Data d) {
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(d);
    return std::move(ss);
}

Data deserialize(std::stringstream& ss) {
    cereal::BinaryInputArchive iarchive(ss);
    Data d2;
    iarchive(d2);
    return d2;
}

TEST_CASE("Serialize/deserialize/reserialize", "[data]") {
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
    SECTION("string") {
        std::string s("abcdef");
        auto d = make_data(s);
        auto binary = d.serializer();
        REQUIRE(binary.size() == 14);
    }

    SECTION("int") {
        auto d = make_data(10);
        auto binary = d.serializer();
        REQUIRE(binary.size() == 4);
    }

    SECTION("double") {
        auto d = make_data(0.015);
        auto binary = d.serializer();
        REQUIRE(binary.size() == 8);
    }
}

#include "catch.hpp"

#include "taskloaf/data.hpp"

using namespace taskloaf;

TEST_CASE("make data", "[data]") {
    auto d = make_data(std::string("yea!"));
    REQUIRE(d.get_as<std::string>() == "yea!");
}

struct MyData
{
    int x;

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(x);
    }
};

TEST_CASE("Cereal", "[data]") {
    std::stringstream ss;

    {
        cereal::BinaryOutputArchive oarchive(ss);
        MyData m1{1}, m2{2}, m3{3};
        oarchive(m1, m2, m3);
    }

    {
        cereal::BinaryInputArchive iarchive(ss);
        MyData m1, m2, m3;
        iarchive(m1, m2, m3);
        REQUIRE(m1.x == 1); REQUIRE(m2.x == 2); REQUIRE(m3.x == 3);
    }
}

TEST_CASE("Serialize/deserialize", "[data]") {
    auto d = make_data(10);
    auto binary = d.serializer();
    {
        cereal::BinaryInputArchive iarchive(binary.stream);
        int x;
        iarchive(x);
        REQUIRE(x == 10);
    }
}

TEST_CASE("Measure serialized size", "[data]") {
    SECTION("string") {
        std::string s("abcdef");
        auto d = make_data(s);
        auto binary = d.serializer();
        REQUIRE(binary.n_bytes() == 14);
    }

    SECTION("int") {
        auto d = make_data(10);
        auto binary = d.serializer();
        REQUIRE(binary.n_bytes() == 4);
    }

    SECTION("double") {
        auto d = make_data(0.015);
        auto binary = d.serializer();
        REQUIRE(binary.n_bytes() == 8);
    }
}


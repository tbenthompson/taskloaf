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

TEST_CASE("Serialize Data", "[data]") {
    auto d = make_data(10);
    d.serialize();
    {
        cereal::BinaryInputArchive iarchive(d.internals->serialized_data);
        int x;
        iarchive(x);
        REQUIRE(x == 10);
    }
}

TEST_CASE("Deserialize data", "[data]") {

}

TEST_CASE("Measure serialized size", "[data]") {
    
}


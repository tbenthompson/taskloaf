#include "catch.hpp"

#include "taskloaf/data.hpp"

using namespace taskloaf;

TEST_CASE("safe void ptr") {
    auto p = make_safe_void_ptr(1.01);
    REQUIRE(p.get_as<double>() == 1.01);
}

TEST_CASE("make data") {
    auto d = make_data(std::string("yea!"));
    REQUIRE(d.get_as<std::string>() == "yea!");
}

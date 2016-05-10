#include "catch.hpp"

#include "taskloaf/execute.hpp"
#include "taskloaf/fnc.hpp"
#include "taskloaf/closure.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Apply args", "[fnc]") {
    std::tuple<double,int> args{1.012, 2};

    SECTION("Lambda") {
        auto out = apply_args([] (double x, int y) { return x * y; }, args);
        REQUIRE(out == 2.024);
    }

    SECTION("Function") {
        Function<double(double,int)> f = [] (double x, int y) { return x * y; };
        auto out = apply_args(f, args);
        REQUIRE(out == 2.024);
    }
}

TEST_CASE("Function lambda", "[fnc]") {
    auto f = make_function([] (int x) { return x * 2; });
    REQUIRE(f(3) == 6);
}

int test_fnc(int x) {
    return x * 2;
}

TEST_CASE("Function func", "[fnc]") {
    auto f = make_function(test_fnc);
    REQUIRE(f(3) == 6);
}

TEST_CASE("Function capture", "[fnc]") {
    int y = 10;
    auto f = make_function([=] (int x) { return y * x; });
    REQUIRE(f(3) == 30);
}

TEST_CASE("Function capture by ref", "[fnc]") {
    int x = 0;
    auto f = make_function([&] (int y) { x = y; });
    f(1);
    REQUIRE(x == 1);
}

TEST_CASE("Closure one param", "[fnc]") {
    Closure<int(int)> c{
        [] (std::vector<Data>& d, int y) { return d[0].get_as<int>() * y; },
        {make_data(12)}
    }; 
    REQUIRE(c(3) == 36);
}

TEST_CASE("Closure three params", "[fnc]") {
    Closure<double(int)> c{
        [] (std::vector<Data>& d, int a) { 
            if (d[2].get_as<bool>()) {
                return d[0].get_as<int>() * d[1].get_as<double>() * a; 
            } else {
                return 1.1;
            }
        }, 
        std::vector<Data>{make_data(12), make_data(3.3), make_data(true)}
    }; 
    REQUIRE(std::fabs(c(3) - 118.8) < 0.001);
}

TEST_CASE("Serialize/deserialize fnc", "[fnc]") {
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    int mult = 256;
    Function<int(int)> f = [=] (int x) { return x * mult; };

    oarchive(f);
    cereal::BinaryInputArchive iarchive(ss);
    Function<int(int)> f2;
    iarchive(f2);

    mult = 255;
    REQUIRE(f(10) == 2560);
    REQUIRE(f2(10) == f(10));
}

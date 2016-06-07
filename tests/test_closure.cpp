#include "catch.hpp"

#include "taskloaf/closure.hpp"

#include "serialize.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("lambda") {
    auto f = Closure<int(int)>([] (int x) { return x * 2; });
    REQUIRE(f(3) == 6);
}

int test_fnc(int x) {
    return x * 2;
}

TEST_CASE("free fnc") {
    auto f = Closure<int(int)>(test_fnc);
    REQUIRE(f(3) == 6);
}

TEST_CASE("capture") {
    int y = 10;
    auto f = Closure<int(int)>([=] (int x) { return y * x; });
    REQUIRE(f(3) == 30);
}

TEST_CASE("capture by ref") {
    int x = 0;
    auto f = Closure<void(int)>([&] (int y) { x = y; });
    f(1);
    REQUIRE(x == 1);
}

TEST_CASE("one param") {
    Closure<int(int)> c([] (int x, int y) { return x * y; }, 12); 
    REQUIRE(c(3) == 36);
}

TEST_CASE("three params") {
    Closure<double(int)> c{
        [] (int a, double b, bool c, int d) { 
            if (c) {
                return a * b * d; 
            } else {
                return 1.1;
            }
        }, 
        12, 3.3, true
    }; 
    REQUIRE(std::fabs(c(3) - 118.8) < 0.001);
}

TEST_CASE("Serialize/deserialize fnc") {
    auto c = Closure<int(int)>(test_fnc);
    auto c2 = deserialize<Closure<int(int)>>(serialize(c));
    REQUIRE(c2(3) == 6);
}

TEST_CASE("Serialize/deserialize lambda") {
    int mult = 256;
    auto c = Closure<int(int)>([=] (int x) { return x * mult; });
    auto s = serialize(c);
    auto c2 = deserialize<decltype(c)>(s);
    REQUIRE(c2(10) == c(10));
}

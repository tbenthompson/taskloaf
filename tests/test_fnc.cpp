#include "catch.hpp"

#include "taskloaf/fnc.hpp"
#include "taskloaf/closure.hpp"

#include "serialize.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("lambda", "[fnc]") {
    auto f = make_closure([] (int x) { return x * 2; });
    REQUIRE(f(3) == 6);
}

int test_fnc(int x) {
    return x * 2;
}

TEST_CASE("free fnc", "[fnc]") {
    auto f = make_closure(test_fnc);
    REQUIRE(f(3) == 6);
}

TEST_CASE("capture", "[fnc]") {
    int y = 10;
    auto f = make_closure([=] (int x) { return y * x; });
    REQUIRE(f(3) == 30);
}

TEST_CASE("capture by ref", "[fnc]") {
    int x = 0;
    auto f = make_closure([&] (int y) { x = y; });
    f(1);
    REQUIRE(x == 1);
}

TEST_CASE("one param", "[fnc]") {
    Closure<int(int)> c([] (int x, int y) { return x * y; }, 12); 
    REQUIRE(c(3) == 36);
}

TEST_CASE("three params", "[fnc]") {
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

TEST_CASE("Serialize/deserialize typed", "[fnc]") {
    int mult = 256;
    auto f = [=] (int x) { return x * mult; };
    mult = 255;
    TypedClosure<int(int), decltype(f)> c(f, std::make_tuple());
    auto s = serialize(c);
    auto c2 = deserialize<decltype(c)>(s);
    REQUIRE(c(10) == 2560);
    REQUIRE(c2(10) == c(10));
}

TEST_CASE("Serialize/deserialize untyped", "[fnc]") {
    int mult = 256;
    auto c = make_closure([=] (int x) { return x * mult; });
    auto s = serialize(c);
    auto c2 = deserialize<decltype(c)>(s);
    REQUIRE(c2(10) == c(10));
}

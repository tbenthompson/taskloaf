#include "catch.hpp"

#include "taskloaf/closure.hpp"

#include "serialize.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("lambda") {
    auto f = closure([] (Data&, int x) { return x * 2; });
    int x = f(ensure_data(3));
    REQUIRE(x == 6);
}

int test_fnc(Data&, int x) {
    return x * 2;
}

TEST_CASE("free fnc") {
    auto f = closure(test_fnc);
    int x = f(ensure_data(3));
    REQUIRE(x == 6);
}

TEST_CASE("capture") {
    int y = 10;
    auto f = closure([=] (Data&, int x) { return y * x; });
    int x = f(ensure_data(3));
    REQUIRE(x == 30);
}

TEST_CASE("capture by ref") {
    int x = 0;
    auto f = closure([&] (Data&, int y) { x = y; return Data{}; });
    f(ensure_data(1));
    REQUIRE(x == 1);
}

TEST_CASE("param") {
    closure c([] (int x, int y) { return x * y; }, 12); 
    int x = c(ensure_data(3));
    REQUIRE(x == 36);
}

TEST_CASE("Serialize/deserialize fnc") {
    auto c = closure(test_fnc);
    auto c2 = deserialize<closure>(serialize(c));
    int x = c2(ensure_data(3));
    REQUIRE(x == 6);
}

TEST_CASE("Serialize/deserialize lambda") {
    int mult = 256;
    auto c = closure([=] (Data&, int x) { return x * mult; });
    auto s = serialize(c);
    auto c2 = deserialize<decltype(c)>(s);
    int x = c(ensure_data(10));
    int y = c2(ensure_data(10));
    REQUIRE(x == y);
}

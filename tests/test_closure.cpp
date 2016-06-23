#include "doctest.h"

#include "taskloaf/closure.hpp"

#include "serialize.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("lambda") {
    auto f = closure([] (ignore, int x) { return x * 2; });
    int x = f(data(3));
    REQUIRE(x == 6);
}

TEST_CASE("capture") {
    int y = 10;
    auto f = closure([y] (ignore, int x) { return y * x; });
    int x = f(data(3));
    REQUIRE(x == 30);
}

TEST_CASE("capture by ref") {
    int x = 0;
    auto f = closure([&] (ignore, int y) { x = y; return ignore{}; });
    f(data(1));
    REQUIRE(x == 1);
}

TEST_CASE("param") {
    auto c = closure([] (int x, int y) { return x * y; }, 12); 
    int x = c(data(3));
    REQUIRE(x == 36);
}

TEST_CASE("Serialize/deserialize lambda") {
    int mult = 256;
    auto c = closure([=] (ignore, int x) { return x * mult; });
    auto s = serialize(c);
    auto c2 = deserialize<decltype(c)>(s);
    int x = c(data(10));
    int y = c2(data(10));
    REQUIRE(x == y);
}

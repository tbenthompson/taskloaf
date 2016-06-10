#include "catch.hpp"

#include "taskloaf/closure.hpp"

#include "serialize.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("lambda") {
    auto f = make_closure([] (int, int x) { return x * 2; });
    int x = f(ensure_data(3));
    REQUIRE(x == 6);
}
// 
// int test_fnc(int, int x) {
//     return x * 2;
// }
// 
// TEST_CASE("free fnc") {
//     auto f = make_closure(test_fnc);
//     int x = f(ensure_data(3));
//     REQUIRE(x == 6);
// }
// 
// TEST_CASE("capture") {
//     int y = 10;
//     auto f = make_closure([=] (int, int x) { return y * x; });
//     int x = f(ensure_data(3));
//     REQUIRE(x == 30);
// }
// 
// TEST_CASE("capture by ref") {
//     int x = 0;
//     auto f = make_closure([&] (int, int y) { x = y; return ensure_any(); });
//     f(ensure_any(1));
//     REQUIRE(x == 1);
// }
// 
// TEST_CASE("param") {
//     auto c = make_closure([] (int x, int y) { return x * y; }, 12); 
//     int x = c(ensure_data(3));
//     REQUIRE(x == 36);
// }
// 
// TEST_CASE("Serialize/deserialize fnc") {
//     auto c = make_closure(test_fnc);
//     auto c2 = deserialize<decltype(c)>(serialize(c));
//     int x = c2(ensure_data(3));
//     REQUIRE(x == 6);
// }
// 
// 
// TEST_CASE("Serialize/deserialize lambda") {
//     int mult = 256;
//     auto c = make_closure([=] (int, int x) { return x * mult; });
//     auto s = serialize(c);
//     auto c2 = deserialize<decltype(c)>(s);
//     int x = c(ensure_any(10));
//     int y = c2(ensure_any(10));
//     REQUIRE(x == y);
// }

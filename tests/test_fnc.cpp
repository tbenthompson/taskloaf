#include "catch.hpp"

#include "fnc.hpp"

using namespace taskloaf;

TEST_CASE("Apply args") {
    std::vector<Data> args;
    auto in = 1.012;
    args.push_back({make_safe_void_ptr(1.012)});
    args.push_back({make_safe_void_ptr(2)});
    auto out = apply_args(args, [] (double x, int y) { return x * y; });
    REQUIRE(out == 2.024);
}

TEST_CASE("Function works") {
    Function<int(int)> f = [] (int x) { return x * 2; };
    REQUIRE(f(3) == 6);
}

TEST_CASE("Function capture") {
    int y = 10;
    Function<int(int)> f = [=] (int x) { return x * y; };
    REQUIRE(f(2) == 20);
}

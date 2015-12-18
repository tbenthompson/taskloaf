#include "catch.hpp"

#include "fnc.hpp"

using namespace taskloaf;

TEST_CASE("Functor by type") {
    auto f = [] () { return 10; };
    CallFunctorByType<decltype(f)> caller;
    REQUIRE(caller() == 10);
}

TEST_CASE("Check registered") {
    auto name = get_fnc_name([] (const std::vector<Data*>&) { return 10; });
    get_fnc_name([] (const std::vector<Data*>&) { return 11; });
    REQUIRE(fnc_registry.count(name) == 1);
}

TEST_CASE("Apply args") {
    std::vector<Data*> args;
    auto in = 1.012;
    Data data{make_safe_void_ptr(1.012)};
    Data data2{make_safe_void_ptr(2)};
    args.push_back(&data);
    args.push_back(&data2);
    auto out = apply_args(args, [] (double x, int y) { return x * y; });
    REQUIRE(out == 2.024);
}

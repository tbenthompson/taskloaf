#include "catch.hpp"

#include "fnc.hpp"

using namespace taskloaf;

TEST_CASE("Functor by type") {
    auto f = [] () { return 10; };
    CallFunctorByType<decltype(f)> caller;
    REQUIRE(caller() == 10);
}

TEST_CASE("Check registered") {
    REQUIRE(fnc_registry.size() == 2);
    auto name = get_fnc_name([] (std::vector<Data*>&) { return 10; });
    get_fnc_name([] (std::vector<Data*>&) { return 11; });
    REQUIRE(fnc_registry.count(name) == 1);
}

TEST_CASE("Apply args") {
    std::vector<Data*> args;
    auto in = 1.012;
    Data data{make_safe_void_ptr(in)};
    args.push_back(&data);
    auto out = apply_args(args, [] (double x) { return x * 2; });
    REQUIRE(out == in * 2);
}

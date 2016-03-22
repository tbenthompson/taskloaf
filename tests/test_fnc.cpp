#include "catch.hpp"

#include "taskloaf/fnc.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Apply args") {
    std::vector<Data> args;
    auto in = 1.012;
    args.push_back(make_data(1.012));
    args.push_back(make_data(2));
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

struct Tracker {
    int copies = 0;
    int moves = 0;

    Tracker() = default;
    Tracker(const Tracker& rhs) {
        copies = rhs.copies + 1;
        moves = rhs.moves;
    }
    Tracker(Tracker&& rhs) {
        copies = rhs.copies;
        moves = rhs.moves + 1;
    }
    Tracker& operator=(Tracker&&) = delete;
    Tracker& operator=(const Tracker&) = delete;
};

void tester(int expected_copies, int expected_moves,
    Function<std::pair<int,int>()> f) 
{
    auto f2 = std::move(f);
    auto motions = f2();
    REQUIRE(motions.first == expected_copies);
    REQUIRE(motions.second == expected_moves);
}

TEST_CASE("Move capture") {
    Tracker input;
    tester(0, 4, [input = std::move(input)] () mutable {
        return std::make_pair(input.copies, input.moves);
    });
}
   
TEST_CASE("Copy capture") {
    Tracker input;
    tester(1, 3, [input] () mutable {
        return std::make_pair(input.copies, input.moves);
    });
}

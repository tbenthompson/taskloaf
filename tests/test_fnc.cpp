#include "catch.hpp"

#include "taskloaf/fnc.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Apply args", "[fnc]") {
    std::vector<Data> args;
    auto in = 1.012;
    args.push_back(make_data(1.012));
    args.push_back(make_data(2));
    auto out = apply_args(args, [] (double x, int y) { return x * y; });
    REQUIRE(out == 2.024);
}

TEST_CASE("Function works", "[fnc]") {
    Function<int(int)> f = [] (int x) { return x * 2; };
    REQUIRE(f(3) == 6);
}

TEST_CASE("Function capture", "[fnc]") {
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

// TEST_CASE("Move capture", "[fnc]") {
//     Tracker input;
//     tester(0, 3, [input = std::move(input)] () mutable {
//         return std::make_pair(input.copies, input.moves);
//     });
// }
//    
// TEST_CASE("Copy capture", "[fnc]") {
//     Tracker input;
//     tester(1, 2, [input] () mutable {
//         return std::make_pair(input.copies, input.moves);
//     });
// }

TEST_CASE("to_string fnc", "[fnc]") {
    Function<int(int)> f = [a = 256] (int x) { return x * a; };
    Function<int(int)> f2; 
    f2.from_string(f.to_string());
    REQUIRE(f2(13) == f(13));
}

TEST_CASE("Serialize/deserialize fnc", "[fnc]") {
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    Function<int(int)> f = [a = 256] (int x) { return x * a; };
    oarchive(f);
    cereal::BinaryInputArchive iarchive(ss);
    Function<int(int)> f2;
    iarchive(f2);
    REQUIRE(f2(10) == f(10));
}

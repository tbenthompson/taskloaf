#include "catch.hpp"

#include "future.hpp"

using namespace taskloaf; 

TEST_CASE("Ready") {
    auto f = ready(1);
}

TEST_CASE("Then") {
    auto f = ready(1);  
    auto f2 = f.then([] (int x) { return x * 2; });
}

TEST_CASE("Unwrap") {
    auto f = ready(1);
    auto f2 = f.then([] (int x) {
        if (x < 5) {
            return ready(10);
        } else {
            return ready(0);
        }
    }).unwrap();
}

TEST_CASE("When all") {
    auto f = ready(2);
    auto f2 = ready(5);
    when_all(f, f2).then([] (int a, int b) { return a * b; });
}

TEST_CASE("Async") {
    auto f = async([] () { return 50; });
    f.then([] (int x) { return x * 2; });
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc") {
    auto f = async(bark);
}


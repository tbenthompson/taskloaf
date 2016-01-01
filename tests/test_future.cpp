#include "catch.hpp"

#include "taskloaf/future.hpp"

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

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc") {
    auto out = ready(2).then(make_sound);
}

TEST_CASE("Then member fnc") {
    struct ABC {
        ABC() {
            auto val = ready(*this).then(std::mem_fn(&ABC::rocks)); 
        }

        std::string rocks() { return "jump!"; }
    };
    ABC abc;
}

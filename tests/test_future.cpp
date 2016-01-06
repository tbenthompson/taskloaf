#include "catch.hpp"

#include "taskloaf/future.hpp"

using namespace taskloaf; 

void future_tester(auto fnc) {
    bool correct = false;
    launch(1, [&] () {
        auto fut = fnc();
        return fut.then([&] (int x) { 
            correct = (x == 1);
            return shutdown();
        });
    });
    REQUIRE(correct);
}

TEST_CASE("Ready") {
    future_tester([] () { return ready(1); });
}

TEST_CASE("Then") {
    future_tester([] () {
        return ready(0).then([] (int x) { return x + 1; }); 
    });
}

TEST_CASE("Unwrap") {
    future_tester([] () {
        auto f = ready(1);  
        return f.then([] (int x) {
            if (x < 5) {
                return ready(1);
            } else {
                return ready(0);
            }
        }).unwrap();
    });
}

TEST_CASE("When all") {
    future_tester([] () {
        return when_all(ready(0.2), ready(5)).then(
            [] (double a, int b) { return static_cast<int>(a * b); }
        );
    });
}

TEST_CASE("Async") {
    future_tester([] () {
        return async([] () { return 1; });
    });
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc") {
    future_tester([] () {
        auto f = async(bark);
        return ready(1);
    });
}

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc") {
    future_tester([] () {
        auto f = ready(2).then(make_sound);;
        return ready(1);
    });
}

TEST_CASE("Then member fnc") {
    struct ABC {
        ABC() {
            auto val = ready(*this).then(std::mem_fn(&ABC::rocks)); 
        }

        std::string rocks() { return "jump!"; }
    };
    future_tester([] () {
        ABC abc;
        return ready(1);
    });
}

TEST_CASE("Run tree once", "[run]") {
    int x = 0;
    launch(1, [&] () {
        auto once_task = async([&] () { 
            x++;
            return 0; 
        });
        return when_all(
            once_task, once_task, once_task
        ).then([] (int, int, int) {
            return shutdown();
        });
    });
    REQUIRE(x == 1);
}

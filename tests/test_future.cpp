#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf; 

template <typename F>
void future_tester(F fnc) {
    bool correct = false;
    launch_local(1, [&] () {
        auto fut = fnc();
        return fut.then([&] (int x) {
            correct = (x == 1);
            return shutdown();
        });
    });
    REQUIRE(correct);
}

TEST_CASE("Ready", "[future]") {
    future_tester([] () { return ready(1); });
}

TEST_CASE("Then", "[future]") {
    future_tester([] () {
        return ready(0).then([] (int x) { return x + 1; }); 
    });
}

TEST_CASE("Unwrap", "[future]") {
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

TEST_CASE("When all", "[future]") {
    future_tester([] () {
        return when_all(ready(0.2), ready(5)).then(
            [] (double a, int b) { return static_cast<int>(a * b); }
        );
    });
}

TEST_CASE("Async", "[future]") {
    future_tester([] () {
        return async([] () { return 1; });
    });
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc", "[future]") {
    future_tester([] () {
        auto f = async(bark);
        return ready(1);
    });
}

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc", "[future]") {
    future_tester([] () {
        auto f = ready(2).then(make_sound);;
        return ready(1);
    });
}

TEST_CASE("Then member fnc", "[future]") {
    struct ABC {
        void plan() {
            auto val = ready(*this).then([] (ABC abc) { return abc.rocks(); });
        }

        std::string rocks() { return "jump!"; }
    };
    future_tester([] () {
        ABC abc;
        abc.plan();
        return ready(1);
    });
}

TEST_CASE("Run tree once", "[future]") {
    int x = 0;
    launch_local(1, [&] () {
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

TEST_CASE("Empty futures", "[future]") {
    launch_local(1, [&] () {
        auto f = async([] () {})
            .then([] () { return 0; })
            .then([] (int) {})
            .then([] () {})
            .then([] () { return shutdown(); });
        return f;
    });
}

TEST_CASE("Outside of launch, just run immediately", "[future]") {
    int x = 0;
    when_all(async([&] { x = 1; })
            .then([&] {x *= 2; return 1; })
        ).then([] (int) { return ready(1); })
        .unwrap()
        .then([&] (int y) { x += y; });
    REQUIRE(x == 3);
}

// TEST_CASE("Empty when_all", "[future]") {
//     int x = 0;
//     launch_local(1, [&] () {
//         auto f = when_all(async([&] {
//             x = 2;
//         })).then([&] {
//             x *= 2;
//         });
//     });
//     REQUIRE(x == 4);
// }

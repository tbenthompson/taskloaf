#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf; 

template <typename FTest, typename FAsync, typename FThen>
void future_tester_helper(FTest fnc, FAsync fasync, FThen fthen) {

    auto ctx = launch_local(2);
    bool correct = false;
    auto fut = fnc(fasync, fthen).then([&] (int x) {
        correct = (x == 1);
    });
    fut.wait();
    REQUIRE(correct);
}

template <typename FTest>
void future_tester(FTest fnc) {
    future_tester_helper(fnc, 
        [] (auto f) { return async(f); }, 
        [] (auto fut, auto f) { return then(fut, f); }
    );
    future_tester_helper(fnc, 
        [] (auto f) { return asyncd(f); }, 
        [] (auto fut, auto f) { return thend(fut, f); }
    );
}

TEST_CASE("Ready", "[future]") {
    future_tester([] (auto, auto) { return ready(1); });
}

TEST_CASE("Then", "[future]") {
    future_tester([] (auto, auto then) {
        return then(ready(0), [] (int x) { return x + 1; }); 
    });
}

TEST_CASE("Unwrap", "[future]") {
    future_tester([] (auto, auto then) {
        auto f = ready(1);  
        return then(f, [] (int x) {
            if (x < 5) {
                return ready(1);
            } else {
                return ready(0);
            }
        }).unwrap();
    });
}

TEST_CASE("When all", "[future]") {
    future_tester([] (auto, auto then) {
        return then(
            when_all(ready(0.2), ready(5)), 
            [] (double a, int b) { return static_cast<int>(a * b); }
        );
    });
}

TEST_CASE("Async", "[future]") {
    future_tester([] (auto async, auto) {
        return async([] () { return 1; });
    });
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc", "[future]") {
    future_tester([] (auto async, auto) {
        auto f = async(bark);
        return ready(1);
    });
}

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc", "[future]") {
    future_tester([] (auto, auto then) {
        auto f = then(ready(2), make_sound);
        return ready(1);
    });
}

struct ABC {
    void plan() {
        auto val = ready(*this).then([] (ABC abc) { return abc.rocks(); });
    }

    template <typename Archive>
    void serialize(Archive&) {}

    std::string rocks() { return "jump!"; }
};

TEST_CASE("Then member fnc", "[future]") {
    future_tester([] (auto, auto) {
        ABC abc;
        abc.plan();
        return ready(1);
    });
}

TEST_CASE("Wait without context", "[future]") {
    auto f = async([] {});
    f.wait();
}

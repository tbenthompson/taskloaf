#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf; 

TEST_CASE("Ready", "[future]") {
    auto ctx = launch_local(2);
    REQUIRE(ready(1).get() == 1);
}

TEST_CASE("Then", "[future]") {
    auto ctx = launch_local(2);
    REQUIRE(ready(0).then([] (int x) { return x + 1; }).get() == 1);
}

TEST_CASE("Unwrap", "[future]") {
    auto ctx = launch_local(2);
    auto x = ready(4).then([] (int x) {
        if (x < 5) {
            return ready(1);
        } else {
            return ready(0);
        }
    }).unwrap().get();
    REQUIRE(x == 1);
}

TEST_CASE("When all", "[future]") {
    auto ctx = launch_local(2);
    auto x = when_all(ready(0.2), ready(5)).then(
        [] (double a, int b) -> int {
            return a * b;
        }).get();
    REQUIRE(x == 1);
}

TEST_CASE("Async", "[future]") {
    auto ctx = launch_local(2);
    REQUIRE(async([] () { return 1; }).get() == 1);
}

std::string bark() { return "arf"; }
TEST_CASE("Async fnc", "[future]") {
    auto ctx = launch_local(2);
    async(bark).wait();
}

std::string make_sound(int x) { return "llamasound" + std::to_string(x); }
TEST_CASE("Then fnc", "[future]") {
    auto ctx = launch_local(2);
    ready(2).then(make_sound).wait();
}

void bark_silently() {}
TEST_CASE("void async fnc", "[future]") {
    auto ctx = launch_local(2);
    async(bark_silently).wait();
}

struct ABC {
    auto plan() {
        return ready(*this).then([] (ABC abc) { return abc.rocks(); });
    }

    template <typename Archive>
    void serialize(Archive&) {}

    std::string rocks() { return "jump!"; }
};

TEST_CASE("Then member fnc", "[future]") {
    auto ctx = launch_local(1);
    ABC abc;
    abc.plan().wait();
}

TEST_CASE("Empty futures", "[empty_future]") {
    auto ctx = launch_local(1);
    auto f = async([] () {})
        .then([] () { return 0; })
        .then([] (int) {})
        .then([] () {});
    f.wait();
}

// TEST_CASE("Wait without context", "[future]") {
//     auto f = async([] {});
//     f.wait();
// }
// TEST_CASE("Empty when_all", "[empty_future]") {
//     int x = 0;
//     when_all(async([&] { x = 2; }), ready(2))
//         .then([&] (int mult) { x *= mult; });
//     REQUIRE(x == 4);
// }
// TEST_CASE("Outside of launch, just run immediately", "[empty_future]") {
//     int x = 0;
//     when_all(
//             async([&] { x = 1; }).then([&] {x *= 2; }),
//             ready(5)
//         ).then([] (int) { return ready(1); })
//         .unwrap()
//         .then([&] (int y) { x += y; });
//     REQUIRE(x == 3);
// }
// 
// TEST_CASE("Copy future outside launch", "[empty_future]") {
//     auto f = ready(5);
//     Future<int> f2;
//     f2 = f;
//     auto result = when_all(f2, f2).then([] (int, int) {});
// }

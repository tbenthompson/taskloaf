#include "doctest.h"

#include "taskloaf/launcher.hpp"
#include "taskloaf/future.hpp"

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

using namespace taskloaf;

TEST_CASE("Ready") {
    auto ctx = launch_local(1);
    auto f = ut_ready(10);
    int x = f.get();
    REQUIRE(x == 10);
}

TEST_CASE("Then") {
    auto ctx = launch_local(1);
    auto f = ut_ready(10);
    int x = f.then([] (ignore&, int x) { return x * 2; }).get();
    REQUIRE(x == 20);
}

TEST_CASE("task") {
    auto ctx = launch_local(1);
    int result = ut_task([] (ignore&, ignore&) { return 30; }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(1);
    std::string x = ut_task([] (ignore&, ignore&) {
        return ut_task([] (ignore&, ignore&) {
            return std::string("HI");
        });
    }).unwrap().get();
    REQUIRE(x == "HI");
}

TEST_CASE("Task elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_worker->get_addr() == address{0});
    int result = task(1, [] {
        REQUIRE(cur_worker->get_addr() == address{1});
        return 30;
    }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Task here") {
    auto ctx = launch_local(2);
    REQUIRE(cur_worker->get_addr() == address{0});
    int result = task(location::here, [] {
        REQUIRE(cur_worker->get_addr() == address{0});
        return 30;
    }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Then elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_worker->get_addr() == address{0});
    int result = ready(2).then(1, [] (int x) {
        REQUIRE(cur_worker->get_addr() == address{1});
        return x + 7; 
    }).get();
    REQUIRE(result == 9);
}

TEST_CASE("Unwrap from elsewhere") {
    auto ctx = launch_local(2);
    int x = task(1, [] {
        return ready(1); 
    }).unwrap().get();
    REQUIRE(x == 1);
}

TEST_CASE("Ready immediate") {
    auto ctx = launch_local(1);
    REQUIRE(ready(10).get() == 10);
}

TEST_CASE("task immediate") {
    auto ctx = launch_local(1);
    REQUIRE(task([] { return 10; }).get() == 10);
}

TEST_CASE("Then immediate") {
    auto ctx = launch_local(1);
    REQUIRE(ready(10).then([] (int x) { return x * 2; }).get() == 20);
}

TEST_CASE("Then immediate with enclosed") {
    auto ctx = launch_local(1);
    int result = ready(10).then(
        [] (int a, int x) { return a + x * 2; },
        2
    ).get();
    REQUIRE(result == 22);
}

TEST_CASE("Pass by reference then") {
    auto ctx = launch_local(1);
    double result = ready(std::vector<double>{2.0, 1.0}).then(
        [] (std::vector<double>& x) {
            return x[0];
        }
    ).get();
    REQUIRE(result == 2.0);
}

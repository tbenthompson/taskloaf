#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"
#include "taskloaf/typed_future.hpp"

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

TEST_CASE("Async") {
    auto ctx = launch_local(1);
    int result = ut_async([] (ignore&, ignore&) { return 30; }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(1);
    std::string x = ut_async([] (ignore&, ignore&) {
        return ut_async([] (ignore&, ignore&) {
            return std::string("HI");
        });
    }).unwrap().get();
    REQUIRE(x == "HI");
}

TEST_CASE("ut_async elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = ut_async(1, [] (_,_) {
        REQUIRE(cur_addr == address{1});
        return 30;
    }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Then elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = ut_ready(2).then(1, [] (_,int x) {
        bool not_0 = cur_addr == address{1};
        REQUIRE(not_0);
        return x + 7; 
    }).get();
    REQUIRE(result == 9);
}

TEST_CASE("Unwrap from elsewhere") {
    auto ctx = launch_local(2);
    int x = ut_async(1, [] (_,_) {
        return ut_ready(1); 
    }).unwrap().get();
    REQUIRE(x == 1);
}

TEST_CASE("Ready immediate") {
    auto ctx = launch_local(1);
    REQUIRE(ready(10).get() == 10);
}

TEST_CASE("Async immediate") {
    auto ctx = launch_local(1);
    REQUIRE(async([] { return 10; }).get() == 10);
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

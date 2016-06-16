#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

#include <cereal/types/string.hpp>

using namespace taskloaf;

TEST_CASE("Ready") {
    auto ctx = launch_local(1);
    auto f = ready(10);
    int x = f.get();
    REQUIRE(x == 10);
}

TEST_CASE("Then") {
    auto ctx = launch_local(1);
    auto f = ready(10);
    int x = f.then([] (ignore&, int x) { return x * 2; }).get();
    REQUIRE(x == 20);
}

TEST_CASE("Async") {
    auto ctx = launch_local(1);
    int result = async([] (ignore&, ignore&) { return 30; }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(1);
    std::string x = async([] (ignore&, ignore&) {
        return async([] (ignore&, ignore&) {
            return std::string("HI");
        });
    }).unwrap().get();
    REQUIRE(x == "HI");
}

TEST_CASE("Async elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = async(1, [] (_,_) {
        REQUIRE(cur_addr == address{1});
        return 30;
    }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Then elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = ready(2).then(1, [] (_,int x) {
        bool not_0 = cur_addr == address{1};
        REQUIRE(not_0);
        return x + 7; 
    }).get();
    REQUIRE(result == 9);
}

TEST_CASE("Unwrap from elsewhere") {
    auto ctx = launch_local(2);
    int x = async(0, [] (_,_) {
        return ready(1); 
    }).unwrap().get();
    REQUIRE(x == 1);
}

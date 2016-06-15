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
    int x = f.then([] (ignore&, int x) { return x * 2; })
        .get();
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

// TEST_CASE("When all") {
//     auto ctx = launch_local(2);
//     int x = when_all({ready(make_data(2)), ready(make_data(3))}).then(
//         [] (std::vector<Data>& d) {
//             return std::vector<Data>{
//                 make_data(d[0].get_as<int>() + d[1].get_as<int>())
//             };
//         }
//     ).get().convertible();
//     REQUIRE(x == 5);
// }

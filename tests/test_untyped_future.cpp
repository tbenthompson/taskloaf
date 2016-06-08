#include "catch.hpp"

#include "taskloaf/untyped_future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf;

TEST_CASE("Ready") {
    auto ctx = launch_local(2);
    auto f = ready(ensure_data(10));
    int x = f.get();
    REQUIRE(x == 10);
}

TEST_CASE("Then") {
    auto ctx = launch_local(2);
    int x = ready(ensure_data(10)).then([] (Data&, int x) { return x * 2; }).get();
    REQUIRE(x == 20);
}

TEST_CASE("Async") {
    auto ctx = launch_local(2);
    int result = async([] (Data&, Data&) { return 30; }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(2);
    int x = async([] (Data&, Data&) {
        return async([] (Data&, Data&) {
            return 40;
        });
    }).unwrap().get();
    REQUIRE(x == 40);
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

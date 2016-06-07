#include "catch.hpp"

#include "taskloaf/untyped_future.hpp"
#include "taskloaf/launcher.hpp"

using namespace taskloaf;

TEST_CASE("Ready") {
    auto ctx = launch_local(2);
    auto f = ready(std::vector<Data>{make_data(10)});
    int x = f.get()[0].convertible();
    REQUIRE(x == 10);
}

TEST_CASE("Then") {
    auto ctx = launch_local(2);
    int x = ready({make_data(10)}).then(ThenTaskT(
        [] (std::vector<Data>& d) { 
            return std::vector<Data>{make_data(d[0].get_as<int>() * 2)};
        }
    )).get()[0].convertible();
    REQUIRE(x == 20);
}

TEST_CASE("Async") {
    auto ctx = launch_local(2);
    int result = async(AsyncTaskT([] () {
        return std::vector<Data>{make_data(30)}; 
    })).get()[0].convertible();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(2);
    int x = async(AsyncTaskT([] () {
        return std::vector<Data>{make_data(
            async(AsyncTaskT([] () {
                return std::vector<Data>{make_data(40)};
            }))
        )};
    })).unwrap().get()[0].convertible();
    REQUIRE(x == 40);
}

TEST_CASE("When all") {
    auto ctx = launch_local(2);
    int x = when_all({ready({make_data(2)}), ready({make_data(3)})}).then(
        [] (std::vector<Data>& d) {
            return std::vector<Data>{
                make_data(d[0].get_as<int>() + d[1].get_as<int>())
            };
        }
    ).get()[0].convertible();
    REQUIRE(x == 5);
}

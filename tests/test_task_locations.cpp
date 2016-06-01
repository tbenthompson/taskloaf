#include "catch.hpp"

#include "taskloaf.hpp"

#include <thread>
#include <chrono>

using namespace taskloaf;

TEST_CASE("Here") {
    auto ctx = launch_local(4);
    Worker* submit_worker = cur_worker;
    for (size_t i = 0; i < 20; i++) {
        async(Loc::here, [=] () {
            REQUIRE(cur_worker == submit_worker);
        }).wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

#include "catch.hpp"

#include "taskloaf/run.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Run ready then") {
    auto plan = ready(10).then([] (int x) {
        REQUIRE(x == 10);
        return shutdown();
    });
    run(plan);
}

TEST_CASE("Run async") {
    auto plan = async([] () {
        return 20;
    }).then([] (int x) {
        REQUIRE(x == 20);   
        return shutdown();
    });
    run(plan);
}

TEST_CASE("Run when all") {
    auto plan = when_all(
        ready(10), ready(20)
    ).then([] (int x, int y) {
        return x < y; 
    }).then([] (bool z) {
        REQUIRE(z);
        return shutdown();
    });
    run(plan);
}

TEST_CASE("Run unwrap") {
    auto plan = ready(10).then([] (int x) {
        if (x < 20) {
            return ready(5);
        } else {
            return ready(1);
        }
    }).unwrap().then([] (int y) {
        REQUIRE(y == 5);
        return shutdown();
    });
    run(plan);
}

TEST_CASE("Run tree once") {
    int x = 0;
    auto once_task = async([&] () { x++; return 0; });
    auto shutdown_task = async([&] () { return shutdown();});
    auto tasks = when_all(shutdown_task, once_task, once_task);
    run(tasks);

    REQUIRE(x == 1);
}

auto move_string(auto in, int count) {
    count--;
    if(count <= 0) {
        return in;
    }
    auto tasks = in.then([] (std::string& val) {
        return std::move(val);
    });
    return move_string(tasks, count);
}

TEST_CASE("Pass by reference") {
    auto tasks = move_string(ready<std::string>("hello"), 200);
    auto runme = tasks.then([] (std::string& val) {
        (void)val; return shutdown();   
    });
    run(runme);
}


#include "catch.hpp"

#include "worker_test_helpers.hpp"

using namespace taskloaf;

TEST_CASE("Number of workers", "[worker]") {
    auto ws = workers(4);
    REQUIRE(ws[0]->n_workers() == 4);
}

void stealing_test(int n_steal_attempts) {
    auto ws = workers(2);
    int x = 0;
    int n_tasks = 5;
    for (int i = 0; i < n_tasks; i++) {
        ws[0]->get_task_collection().add_task({
            [&] (std::vector<Data>&) {
                x = 1; 
            },
            {}
        });
    }
    settle(ws);
    for (int i = 0; i < n_steal_attempts; i++) {
        ws[1]->get_task_collection().steal();
    }
    REQUIRE(ws[0]->get_task_collection().size() == n_tasks);
    REQUIRE(ws[1]->get_task_collection().size() == 0);
    settle(ws);
    REQUIRE(ws[0]->get_task_collection().size() == n_tasks - 1);
    REQUIRE(ws[1]->get_task_collection().size() == 1);
}

TEST_CASE("Two workers one steal", "[worker]") {
    stealing_test(1);
}

TEST_CASE("Two workers two steals = second does nothing", "[worker]") {
    stealing_test(2);
}

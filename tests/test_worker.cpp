#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"
#include "taskloaf/default_worker.hpp"
#include "taskloaf/comm.hpp"

#include <thread>
#include <chrono>

using namespace taskloaf;

TEST_CASE("Launch and immediately destroy", "[launch]") {
    launch_local(4);
}

TEST_CASE("Run here") {
    auto ctx = launch_local(4);
    auto* submit_worker = cur_worker;
    for (size_t i = 0; i < 5; i++) {
        async(location::here, closure([=] (ignore, ignore) {
            REQUIRE(cur_worker == submit_worker);
            return ignore{};
        })).get();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct testing_comm: public comm {
    address addr = {0};
    std::vector<address> remotes = {{1}};
    std::vector<closure> sent;

    const address& get_addr() const { return addr; }
    const std::vector<address>& remote_endpoints() { return remotes; }
    closure recv() { return closure(); }

    void send(const address&, closure msg) {
        sent.push_back(std::move(msg));
    }
};

#define MAKE_TASK_COLLECTION(name)\
    logger my_log({0});\
    testing_comm comm;\
    task_collection name(my_log, comm);\

TEST_CASE("Add task") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task(closure([&] (ignore, ignore) { x = 1; return ignore{}; }));
    REQUIRE(tc.size() == 1);
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Victimized") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task(closure([&] (ignore, ignore) { return ignore{}; }));
    REQUIRE(tc.size() == 1);
    auto tasks = tc.victimized();
    REQUIRE(tc.size() == 0);
    REQUIRE(tasks.size() == 1);
}

TEST_CASE("Receive tasks") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task(closure([&] (ignore, ignore) { return ignore{}; }));
    auto tasks = tc.victimized();
    tc.receive_tasks(std::move(tasks));
    REQUIRE(tc.size() == 1);
}

TEST_CASE("Steal request") {
    MAKE_TASK_COLLECTION(tc);
    tc.steal();
    REQUIRE(comm.sent.size() == 1);
    tc.steal();
    REQUIRE(comm.sent.size() == 1);
}

TEST_CASE("Stealable tasks are LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task(closure([&] (ignore, ignore) { x = 1; return ignore{}; }));
    tc.add_task(closure([&] (ignore, ignore) { x *= 2; return ignore{}; }));
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Local tasks can't be stolen") {
    MAKE_TASK_COLLECTION(tc);
    tc.add_local_task(closure([&] (ignore, ignore) { return ignore{}; }));
    REQUIRE(tc.size() == 1);
    REQUIRE(tc.victimized().size() == 0);
}

TEST_CASE("Mixed tasks run LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task(closure([&] (ignore&,ignore&) { x = 1; return ignore{}; }));
    tc.add_local_task(closure([&] (ignore&,ignore&) { x *= 2; return ignore{}; }));
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Send remotely assigned task") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({1}, closure([&] (ignore&,ignore&) { x = 1; return ignore{}; }));
    REQUIRE(comm.sent.size() == 1);
    tc.add_local_task(std::move(comm.sent[0]));
    tc.run_next();
    REQUIRE(x == 1);
}

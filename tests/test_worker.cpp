#include "doctest.h"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"
#include "taskloaf/default_worker.hpp"
#include "taskloaf/comm.hpp"
#include "taskloaf/local_comm.hpp"

#include <cereal/types/vector.hpp>

#include <thread>
#include <chrono>

using namespace taskloaf;

TEST_CASE("Launch and immediately destroy") {
    launch_local(4);
}

TEST_CASE("Run here") {
    auto ctx = launch_local(4);
    auto* submit_worker = cur_worker;
    for (size_t i = 0; i < 5; i++) {
        ut_async(location::here, closure([=] (_, _) {
            REQUIRE(cur_worker == submit_worker);
            return _{};
        })).get();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST_CASE("Steal real") {
    local_comm_queues lcq(2);
    std::vector<std::unique_ptr<default_worker>> g;
    for (size_t i = 0; i < 2; i++) {
        g.emplace_back(std::make_unique<default_worker>(
            std::make_unique<local_comm>(lcq, i)
        ));
    }
    auto& w0 = *g[0];
    auto& w1 = *g[1];

    auto run_on = [&] (size_t i, auto f) {
        set_cur_worker(g[i].get());
        f();
    };

    data d(std::vector<double>{});

    run_on(0, [&] {
        w0.add_task(closure([] (_,_) {
            REQUIRE(cur_addr == address{1}); return _{}; 
        }, d));
    });

    run_on(1, [&] { w1.one_step(); });

    run_on(0, [&] { 
        w0.one_step(); 
        w0.add_task([] (_,_) {
            REQUIRE(cur_addr == address{0}); return _{}; 
        });
        w0.one_step(); 
    });

    run_on(1, [&] { 
        w1.one_step(); 
        w1.one_step(); 
    });

    run_on(0, [&] { 
        w0.one_step(); 
        w0.one_step(); 
    });

    for (size_t i = 0; i < 10; i++) {
        run_on(0, [&] { w0.one_step(); });
        run_on(1, [&] { w1.one_step(); });
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
    tc.add_task(closure([&] (_, _) { x = 1; return _{}; }));
    REQUIRE(int(tc.size()) == 1);
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Victimized") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task(closure([&] (_, _) { return _{}; }));
    REQUIRE(int(tc.size()) == 1);
    auto tasks = tc.victimized();
    REQUIRE(int(tc.size()) == 0);
    REQUIRE(int(tasks.size()) == 1);
}

TEST_CASE("Receive tasks") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task(closure([&] (_, _) { return _{}; }));
    auto tasks = tc.victimized();
    tc.receive_tasks(std::move(tasks));
    REQUIRE(int(tc.size()) == 1);
}

TEST_CASE("Steal request") {
    MAKE_TASK_COLLECTION(tc);
    tc.steal();
    REQUIRE(int(comm.sent.size()) == 1);
    tc.steal();
    REQUIRE(int(comm.sent.size()) == 1);
}

TEST_CASE("Stealable tasks are LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task(closure([&] (_, _) { x = 1; return _{}; }));
    tc.add_task(closure([&] (_, _) { x *= 2; return _{}; }));
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Local tasks can't be stolen") {
    MAKE_TASK_COLLECTION(tc);
    tc.add_local_task(closure([&] (_, _) { return _{}; }));
    REQUIRE(int(tc.size()) == 1);
    REQUIRE(int(tc.victimized().size()) == 0);
}

TEST_CASE("Mixed tasks run LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task(closure([&] (_&,_&) { x = 1; return _{}; }));
    tc.add_local_task(closure([&] (_&,_&) { x *= 2; return _{}; }));
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Send remotely assigned task") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({1}, closure([&] (_&,_&) { x = 1; return _{}; }));
    REQUIRE(int(comm.sent.size()) == 1);
    tc.add_local_task(std::move(comm.sent[0]));
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Within a task, new tasks are FILO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task([&] (_,_) { 
        tc.add_task([&] (_,_) { x += 1; return _{}; });
        tc.add_task([&] (_,_) { x *= 2; return _{}; });
        return _{};
    });
    for (int i = 0; i < 3; i++) {
        tc.run_next();
    }
    REQUIRE(x == 2);
}


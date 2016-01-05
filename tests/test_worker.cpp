#include "catch.hpp"

#include "taskloaf/worker.hpp"
#include "taskloaf/id.hpp"
#include "taskloaf/comm.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Worker") {
    Worker w;
    int x = 0;
    w.add_task([&] () { x = 1; cur_worker->shutdown(); });
    w.run();
    REQUIRE(x == 1);
}

void stealing_test(int n_steals) {
    Worker w1;
    Worker w2;
    int x = 0;
    int n_tasks = 5;
    for (int i = 0; i < n_tasks; i++) {
        w1.add_task([&] () { x = 1; });
    }
    w2.introduce(w1.get_addr());
    for (int i = 0; i < n_steals; i++) {
        w2.tasks.steal();
    }
    REQUIRE(w1.tasks.size() == n_tasks);
    REQUIRE(w2.tasks.size() == 0);
    w1.recv();
    REQUIRE(w1.tasks.size() == n_tasks - 1);
    w2.recv();
    REQUIRE(w2.tasks.size() == 1);
}

TEST_CASE("Two workers one steal") {
    stealing_test(1);
}

TEST_CASE("Two workers two steals = second does nothing") {
    stealing_test(2);
}

TEST_CASE("Dec ref deletes") {
    Worker w; 
    cur_worker = &w;
    {
        auto ivarref = w.new_ivar(new_id()).first;
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}

TEST_CASE("Inc ref preserves") {
    Worker w;
    cur_worker = &w;
    std::unique_ptr<IVarRef> ivarref2;
    {
        auto ivarref = w.new_ivar(new_id()).first;
        ivarref2.reset(new IVarRef(ivarref));
    }
    REQUIRE(w.ivar_tracker.n_owned() == 1);
}

std::vector<Worker> workers() {
    std::vector<Worker> ws;
    ws.emplace_back();
    ws.emplace_back();
    ws[1].introduce(ws[0].get_addr());
    ws[0].recv();
    ws[1].recv();
    return std::move(ws);
}

ID id_on_worker(const Worker& w) {
    auto id = w.ivar_tracker.get_ring_locs()[0];
    id.secondhalf--;
    return id;
}

void settle(std::vector<Worker>& ws) {
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < ws.size(); j++) {
            cur_worker = &ws[j];
            ws[j].recv();
        }
    }
}

TEST_CASE("Remote reference counting") {
    auto ws = workers();
    {
        auto id = id_on_worker(ws[1]);
        cur_worker = &ws[0];
        auto iv = ws[1].new_ivar(id);
        (void)iv;

        cur_worker = &ws[1];
        ws[1].recv();

        REQUIRE(ws[1].ivar_tracker.n_owned() == 1);
        cur_worker = &ws[1];
    }
    cur_worker = &ws[1];
    ws[1].recv();
    REQUIRE(ws[1].ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting change location") {
    auto ws = workers();
    {
        auto id = id_on_worker(ws[1]);

        cur_worker = &ws[0];
        auto iv = ws[0].new_ivar(id);
        (void)iv;

        ws[1].recv();

        REQUIRE(ws[1].ivar_tracker.n_owned() == 1);
        cur_worker = &ws[1];
    }
    REQUIRE(ws[1].ivar_tracker.n_owned() == 0);
}

void remote(int owner_worker, int fulfill_worker,
    int trigger_worker, bool trigger_first) 
{
    auto ws = workers();
    int x = 0;

    auto id = id_on_worker(ws[owner_worker]);
    cur_worker = &ws[owner_worker];
    auto iv = ws[owner_worker].new_ivar(id).first;

    if (trigger_first) {
        cur_worker = &ws[trigger_worker];
        ws[trigger_worker].add_trigger(iv, [&] (std::vector<Data>& vals) {
            x = vals[0].get_as<int>(); 
        });
        settle(ws);
    }

    cur_worker = &ws[fulfill_worker];
    ws[fulfill_worker].fulfill(iv, {Data{make_safe_void_ptr(1)}});

    if (!trigger_first) {
        cur_worker = &ws[trigger_worker];
        ws[trigger_worker].add_trigger(iv, [&] (std::vector<Data>& vals) {
            x = vals[0].get_as<int>(); 
        });
    }

    if (owner_worker == fulfill_worker && fulfill_worker == trigger_worker) {
        REQUIRE(x == 1);
        return;
    } else {
        REQUIRE(x == 0);
        settle(ws);
        REQUIRE(x == 1);
    }
}

TEST_CASE("remote fulfill local pre-trigger") {
    remote(0, 1, 1, true);
}

TEST_CASE("remote fulfill remote pre-trigger") {
    remote(0, 1, 1, true);
}

TEST_CASE("local fulfill remote pre-trigger") {
    remote(0, 0, 1, true);
}

TEST_CASE("local fulfill local pre-trigger") {
    remote(0, 0, 0, true);
}

TEST_CASE("remote fulfill local post-trigger") {
    remote(0, 1, 1, false);
}

TEST_CASE("remote fulfill remote post-trigger") {
    remote(0, 1, 1, false);
}

TEST_CASE("local fulfill remote post-trigger") {
    remote(0, 0, 1, false);
}

TEST_CASE("local fulfill local post-trigger") {
    remote(0, 0, 0, false);
}

//TODO: Test for remote deleting of vals and triggers.

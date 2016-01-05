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

void settle(std::vector<Worker>& ws) {
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < ws.size(); j++) {
            cur_worker = &ws[j];
            ws[j].recv();
        }
    }
}

std::vector<Worker> workers(int n_workers) {
    std::vector<Worker> ws;
    for (int i = 0; i < n_workers; i++) {
        ws.emplace_back();
        if (i != 0) {
            ws[i].introduce(ws[0].get_addr());
        }
        settle(ws);
    }
    return std::move(ws);
}

ID id_on_worker(const Worker& w) {
    auto id = w.ivar_tracker.get_ring_locs()[0];
    id.secondhalf--;
    return id;
}

TEST_CASE("Remote reference counting") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);
        cur_worker = &ws[0];
        auto iv = ws[1].new_ivar(id);
        (void)iv;

        cur_worker = &ws[1];
        ws[1].recv();

        ws[0].fulfill(iv.first, {make_data(1)});
        REQUIRE(ws[0].ivar_tracker.n_vals_here() == 1);
        ws[1].recv();

        REQUIRE(ws[1].ivar_tracker.n_owned() == 1);
        cur_worker = &ws[1];
    }
    cur_worker = &ws[1];
    settle(ws);
    REQUIRE(ws[0].ivar_tracker.n_vals_here() == 0);
    REQUIRE(ws[1].ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting change location") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);

        cur_worker = &ws[0];
        auto iv = ws[0].new_ivar(id);

        ws[1].add_trigger(iv.first, [] (std::vector<Data>&) {});
        ws[0].add_trigger(iv.first, [] (std::vector<Data>&) {});
        REQUIRE(ws[0].ivar_tracker.n_triggers_here() == 1);
        REQUIRE(ws[1].ivar_tracker.n_triggers_here() == 1);

        ws[1].recv();

        REQUIRE(ws[1].ivar_tracker.n_owned() == 1);
        cur_worker = &ws[1];
    }
    settle(ws);
    REQUIRE(ws[0].ivar_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1].ivar_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1].ivar_tracker.n_owned() == 0);
}

void remote(int n_workers, int owner_worker, int fulfill_worker,
    int trigger_worker, bool trigger_first) 
{
    std::cout << n_workers << " " << owner_worker << " " << fulfill_worker << " " << trigger_worker << " " << trigger_first << std::endl;
    auto ws = workers(n_workers);
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
    settle(ws);

    if (!trigger_first) {
        cur_worker = &ws[trigger_worker];
        ws[trigger_worker].add_trigger(iv, [&] (std::vector<Data>& vals) {
            x = vals[0].get_as<int>(); 
        });
        settle(ws);
    }

    REQUIRE(x == 1);
}

TEST_CASE("fulfill triggers") {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 2; k++) {
                remote(std::max(j + 1, 2), 0, i, j, k == 0);
            }
        }
    }
}

#include "catch.hpp"

#include "taskloaf/worker.hpp"
#include "taskloaf/id.hpp"
#include "taskloaf/comm.hpp"
#include "taskloaf/local_comm.hpp"

#include <iostream>

using namespace taskloaf;

void settle(std::vector<std::unique_ptr<Worker>>& ws) {
    for (int i = 0; i < 10; i++) {
        for (size_t j = 0; j < ws.size(); j++) {
            cur_worker = ws[j].get();
            ws[j]->recv();
        }
    }
}

Worker worker() {
    auto lcq = std::make_shared<LocalCommQueues>(1);
    return Worker(std::make_unique<LocalComm>(LocalComm(lcq, 0)));
}

std::vector<std::unique_ptr<Worker>> workers(int n_workers) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    std::vector<std::unique_ptr<Worker>> ws;
    for (int i = 0; i < n_workers; i++) {
        ws.emplace_back(std::make_unique<Worker>(
            std::make_unique<LocalComm>(LocalComm(lcq, i))
        ));
        if (i != 0) {
            ws[i]->introduce(ws[0]->get_addr());
        }
        settle(ws);
    }
    return std::move(ws);
}

ID id_on_worker(const std::unique_ptr<Worker>& w) {
    auto id = w->ivar_tracker.get_ring_locs()[0];
    id.secondhalf++;
    return id;
}

TEST_CASE("Worker") {
    auto w = worker();
    int x = 0;
    w.add_task({
        [] (std::vector<Data>& d) {
            d[0].get_as<std::reference_wrapper<int>>().get() = 1; 
            cur_worker->shutdown(); 
        }, 
        {make_data(std::ref(x))}
    });
    w.run();
    REQUIRE(x == 1);
}

void stealing_test(int n_steals) {
    auto ws = workers(2);
    int x = 0;
    int n_tasks = 5;
    for (int i = 0; i < n_tasks; i++) {
        ws[0]->add_task({
            [] (std::vector<Data>& d) {
                d[0].get_as<std::reference_wrapper<int>>().get() = 1; 
            },
            {make_data(std::ref(x))}
        });
    }
    // ws[1]->introduce(ws[0]->get_addr());
    settle(ws);
    for (int i = 0; i < n_steals; i++) {
        ws[1]->tasks.steal();
    }
    REQUIRE(ws[0]->tasks.size() == n_tasks);
    REQUIRE(ws[1]->tasks.size() == 0);
    settle(ws);
    REQUIRE(ws[0]->tasks.size() == n_tasks - 1);
    REQUIRE(ws[1]->tasks.size() == 1);
}

TEST_CASE("Two workers one steal") {
    stealing_test(1);
}

TEST_CASE("Two workers two steals = second does nothing") {
    stealing_test(2);
}

void make_ivar_live(Worker& w, const IVarRef& ivar) {
    w.fulfill(ivar, {make_data(1)});
}

TEST_CASE("Ref tracking destructor deletes") {
    auto w = worker();
    cur_worker = &w;
    {
        IVarRef iv(new_id());
        make_ivar_live(w, iv);
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy constructor") {
    auto w = worker();
    cur_worker = &w;
    {
        std::unique_ptr<IVarRef> ivarref2;
        {
            IVarRef iv(new_id());
            ivarref2.reset(new IVarRef(iv));
        }
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy assignment") {
    auto w = worker();
    cur_worker = &w;
    {
        IVarRef ivarref2;
        {
            IVarRef iv(new_id());
            ivarref2 = iv;
        }
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}


TEST_CASE("Ref tracking empty") {
    IVarRef iv;
}

TEST_CASE("Ref tracking move constructor") {
    auto w = worker();
    cur_worker = &w;
    {
        std::unique_ptr<IVarRef> ivarref2;
        {
            IVarRef iv(new_id());
            make_ivar_live(w, iv);
            ivarref2.reset(new IVarRef(std::move(iv)));
            REQUIRE(iv.empty == true);
        }
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking move assignment") {
    auto w = worker();
    cur_worker = &w;
    {
        IVarRef iv;
        {
            IVarRef iv2(new_id());
            make_ivar_live(w, iv2);
            iv = std::move(iv2);
            REQUIRE(iv2.empty == true);
        }
        REQUIRE(w.ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w.ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);
        cur_worker = ws[0].get();
        IVarRef iv(id);
        (void)iv;

        cur_worker = ws[1].get();
        settle(ws);

        ws[0]->fulfill(iv, {make_data(1)});
        REQUIRE(ws[0]->ivar_tracker.n_vals_here() == 1);
        settle(ws);

        REQUIRE(ws[1]->ivar_tracker.n_owned() == 1);
        cur_worker = ws[1].get();
    }
    cur_worker = ws[1].get();
    settle(ws);
    REQUIRE(ws[0]->ivar_tracker.n_vals_here() == 0);
    REQUIRE(ws[1]->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting change location") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);

        cur_worker = ws[0].get();
        IVarRef iv(id);

        ws[1]->add_trigger(iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}});
        ws[0]->add_trigger(iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}});
        REQUIRE(ws[0]->ivar_tracker.n_triggers_here() == 1);
        REQUIRE(ws[1]->ivar_tracker.n_triggers_here() == 1);

        ws[1]->recv();

        REQUIRE(ws[1]->ivar_tracker.n_owned() == 1);
        cur_worker = ws[1].get();
    }
    settle(ws);
    REQUIRE(ws[0]->ivar_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1]->ivar_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1]->ivar_tracker.n_owned() == 0);
}

void remote(int n_workers, int owner_worker, int fulfill_worker,
    int trigger_worker, bool trigger_first) 
{
    auto ws = workers(n_workers);
    int x = 0;

    auto id = id_on_worker(ws[owner_worker]);
    cur_worker = ws[owner_worker].get();
    IVarRef iv(id);

    auto setup_trigger = [&] () {
        ws[trigger_worker]->add_trigger(iv, {
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                d[0].get_as<std::reference_wrapper<int>>().get() = vals[0].get_as<int>(); 
            },
            {make_data(std::ref(x))}
        });
    };

    if (trigger_first) {
        cur_worker = ws[trigger_worker].get();
        setup_trigger();
        settle(ws);
    }

    cur_worker = ws[fulfill_worker].get();
    ws[fulfill_worker]->fulfill(iv, {make_data(1)});
    settle(ws);

    if (!trigger_first) {
        cur_worker = ws[trigger_worker].get();
        setup_trigger();
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

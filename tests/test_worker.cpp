#include "catch.hpp"

#include "taskloaf/default_worker.hpp"
#include "taskloaf/id.hpp"
#include "taskloaf/comm.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/serializing_comm.hpp"

#include <iostream>

using namespace taskloaf;

void settle(std::vector<std::unique_ptr<DefaultWorker>>& ws) {
    for (int i = 0; i < 10; i++) {
        for (size_t j = 0; j < ws.size(); j++) {
            cur_worker = ws[j].get();
            ws[j]->recv();
        }
    }
}

std::unique_ptr<DefaultWorker> worker() {
    auto lcq = std::make_shared<LocalCommQueues>(1);
    return std::make_unique<DefaultWorker>(std::make_unique<LocalComm>(lcq, 0));
}

std::vector<std::unique_ptr<DefaultWorker>> workers(int n_workers) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    std::vector<std::unique_ptr<DefaultWorker>> ws;
    for (int i = 0; i < n_workers; i++) {
        ws.emplace_back(std::make_unique<DefaultWorker>(
            std::make_unique<SerializingComm>(
                std::make_unique<LocalComm>(lcq, i)
            )
        ));
        if (i != 0) {
            ws[i]->introduce(ws[0]->get_addr());
        }
        ws[i]->core_id = i;
        settle(ws);
    }
    return ws;
}

ID id_on_worker(const std::unique_ptr<DefaultWorker>& w) {
    auto id = w->ivar_tracker.get_ring_locs()[0];
    id.secondhalf++;
    return id;
}

TEST_CASE("DefaultWorker", "[worker]") {
    auto w = worker();
    int x = 0;
    w->add_task({
        [&] (std::vector<Data>&) {
            x = 1; 
            cur_worker->shutdown(); 
        }, 
        {}
    });
    w->run();
    REQUIRE(x == 1);
}

TEST_CASE("Number of workers", "[worker]") {
    auto ws = workers(4);
    REQUIRE(ws[0]->n_workers() == 4);
}

void stealing_test(int n_steals) {
    auto ws = workers(2);
    int x = 0;
    int n_tasks = 5;
    for (int i = 0; i < n_tasks; i++) {
        ws[0]->add_task({
            [&] (std::vector<Data>&) {
                x = 1; 
            },
            {}
        });
    }
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

TEST_CASE("Two workers one steal", "[worker]") {
    stealing_test(1);
}

TEST_CASE("Two workers two steals = second does nothing", "[worker]") {
    stealing_test(2);
}

void make_ivar_live(DefaultWorker& w, const IVarRef& ivar) {
    w.get_ivar_tracker().fulfill(ivar, {make_data(1)});
}

TEST_CASE("Ref tracking destructor deletes", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        IVarRef iv(new_id());
        make_ivar_live(*w, iv);
        REQUIRE(w->ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy constructor", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        std::unique_ptr<IVarRef> ivarref2;
        {
            IVarRef iv(new_id());
            ivarref2.reset(new IVarRef(iv));
        }
        REQUIRE(w->ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy assignment", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        IVarRef ivarref2;
        {
            IVarRef iv(new_id());
            ivarref2 = iv;
        }
        REQUIRE(w->ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w->ivar_tracker.n_owned() == 0);
}


TEST_CASE("Ref tracking empty", "[worker]") {
    IVarRef iv;
}

TEST_CASE("Ref tracking move constructor", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        std::unique_ptr<IVarRef> ivarref2;
        {
            IVarRef iv(new_id());
            make_ivar_live(*w, iv);
            ivarref2.reset(new IVarRef(std::move(iv)));
            REQUIRE(iv.empty == true);
        }
        REQUIRE(w->ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking move assignment", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        IVarRef iv;
        {
            IVarRef iv2(new_id());
            make_ivar_live(*w, iv2);
            iv = std::move(iv2);
            REQUIRE(iv2.empty == true);
        }
        REQUIRE(w->ivar_tracker.n_owned() == 1);
    }
    REQUIRE(w->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting", "[worker]") {
    auto ws = workers(2);
    settle(ws);
    {
        auto id = id_on_worker(ws[1]);
        IVarRef iv(id);
        settle(ws);
        REQUIRE(ws[0]->ivar_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ivar_tracker.n_owned() == 0);

        cur_worker = ws[0].get();
        ws[0]->get_ivar_tracker().fulfill(iv, {make_data(1)});

        REQUIRE(ws[0]->ivar_tracker.n_vals_here() == 1);
        REQUIRE(ws[0]->ivar_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ivar_tracker.n_vals_here() == 0);
        REQUIRE(ws[1]->ivar_tracker.n_owned() == 0);

        settle(ws);
        REQUIRE(ws[0]->ivar_tracker.n_vals_here() == 1);
        REQUIRE(ws[0]->ivar_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ivar_tracker.n_vals_here() == 0);
        REQUIRE(ws[1]->ivar_tracker.n_owned() == 1);

        cur_worker = ws[1].get();
    }
    settle(ws);
    REQUIRE(ws[0]->ivar_tracker.n_vals_here() == 0);
    REQUIRE(ws[0]->ivar_tracker.n_owned() == 0);
    REQUIRE(ws[1]->ivar_tracker.n_vals_here() == 0);
    REQUIRE(ws[1]->ivar_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting change location", "[worker]") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);

        cur_worker = ws[0].get();
        IVarRef iv(id);

        ws[1]->get_ivar_tracker().add_trigger(
            iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}}
        );
        ws[0]->get_ivar_tracker().add_trigger(iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}});
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
    int trigger_worker, bool trigger_first, bool always_settle) 
{
    auto ws = workers(n_workers);
    int x = 0;

    auto id = id_on_worker(ws[owner_worker]);
    IVarRef iv(id);

    auto trigger = [&] () {
        ws[trigger_worker]->get_ivar_tracker().add_trigger(iv, {
            [&] (std::vector<Data>&, std::vector<Data>& vals) {
                x = vals[0].get_as<int>(); 
            },
            {}
        });
        if (always_settle || trigger_worker != fulfill_worker) {
            settle(ws);
        }
    };

    if (trigger_first) {
        trigger();
    }

    ws[fulfill_worker]->get_ivar_tracker().fulfill(iv, {make_data(1)});

    if (always_settle || trigger_worker != fulfill_worker) {
        settle(ws);
    }

    if (!trigger_first) {
        trigger();
    }

    REQUIRE(x == 1);
}

void fulfill_triggers_test(bool always_settle) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 2; k++) {
                remote(std::max(j + 1, 2), 0, i, j, k == 0, always_settle);
            }
        }
    }
}

TEST_CASE("fulfill triggers", "[worker]") {
    fulfill_triggers_test(false);
}

TEST_CASE("fulfill triggers with settling", "[worker]") {
    fulfill_triggers_test(true);
}

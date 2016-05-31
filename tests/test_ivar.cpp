#include "catch.hpp"

#include "worker_test_helpers.hpp"

using namespace taskloaf;

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

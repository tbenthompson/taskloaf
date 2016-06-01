#include "catch.hpp"

#include "worker_test_helpers.hpp"

using namespace taskloaf;

void make_gref_live(DefaultWorker& w, const GlobalRef& gref) {
    w.get_ref_tracker().fulfill(gref, {make_data(1)});
}

TEST_CASE("Ref tracking destructor deletes", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        GlobalRef iv(new_id());
        make_gref_live(*w, iv);
        REQUIRE(w->ref_tracker.n_owned() == 1);
    }
    REQUIRE(w->ref_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy constructor", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        std::unique_ptr<GlobalRef> grefref2;
        {
            GlobalRef iv(new_id());
            grefref2.reset(new GlobalRef(iv));
        }
        REQUIRE(w->ref_tracker.n_owned() == 1);
    }
    REQUIRE(w->ref_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking copy assignment", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        GlobalRef grefref2;
        {
            GlobalRef iv(new_id());
            grefref2 = iv;
        }
        REQUIRE(w->ref_tracker.n_owned() == 1);
    }
    REQUIRE(w->ref_tracker.n_owned() == 0);
}


TEST_CASE("Ref tracking empty", "[worker]") {
    GlobalRef iv;
}

TEST_CASE("Ref tracking move constructor", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        std::unique_ptr<GlobalRef> grefref2;
        {
            GlobalRef iv(new_id());
            make_gref_live(*w, iv);
            grefref2.reset(new GlobalRef(std::move(iv)));
            REQUIRE(iv.empty == true);
        }
        REQUIRE(w->ref_tracker.n_owned() == 1);
    }
    REQUIRE(w->ref_tracker.n_owned() == 0);
}

TEST_CASE("Ref tracking move assignment", "[worker]") {
    auto w = worker();
    cur_worker = w.get();
    {
        GlobalRef iv;
        {
            GlobalRef iv2(new_id());
            make_gref_live(*w, iv2);
            iv = std::move(iv2);
            REQUIRE(iv2.empty == true);
        }
        REQUIRE(w->ref_tracker.n_owned() == 1);
    }
    REQUIRE(w->ref_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting", "[worker]") {
    auto ws = workers(2);
    settle(ws);
    {
        auto id = id_on_worker(ws[1]);
        GlobalRef iv(id);
        settle(ws);
        REQUIRE(ws[0]->ref_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ref_tracker.n_owned() == 0);

        cur_worker = ws[0].get();
        ws[0]->get_ref_tracker().fulfill(iv, {make_data(1)});

        REQUIRE(ws[0]->ref_tracker.n_vals_here() == 1);
        REQUIRE(ws[0]->ref_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ref_tracker.n_vals_here() == 0);
        REQUIRE(ws[1]->ref_tracker.n_owned() == 0);

        settle(ws);
        REQUIRE(ws[0]->ref_tracker.n_vals_here() == 1);
        REQUIRE(ws[0]->ref_tracker.n_owned() == 0);
        REQUIRE(ws[1]->ref_tracker.n_vals_here() == 0);
        REQUIRE(ws[1]->ref_tracker.n_owned() == 1);

        cur_worker = ws[1].get();
    }
    settle(ws);
    REQUIRE(ws[0]->ref_tracker.n_vals_here() == 0);
    REQUIRE(ws[0]->ref_tracker.n_owned() == 0);
    REQUIRE(ws[1]->ref_tracker.n_vals_here() == 0);
    REQUIRE(ws[1]->ref_tracker.n_owned() == 0);
}

TEST_CASE("Remote reference counting change location", "[worker]") {
    auto ws = workers(2);
    {
        auto id = id_on_worker(ws[1]);

        cur_worker = ws[0].get();
        GlobalRef iv(id);

        ws[1]->get_ref_tracker().add_trigger(
            iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}}
        );
        ws[0]->get_ref_tracker().add_trigger(iv, {[] (std::vector<Data>&, std::vector<Data>&) {}, {}});
        REQUIRE(ws[0]->ref_tracker.n_triggers_here() == 1);
        REQUIRE(ws[1]->ref_tracker.n_triggers_here() == 1);

        ws[1]->recv();

        REQUIRE(ws[1]->ref_tracker.n_owned() == 1);
        cur_worker = ws[1].get();
    }
    settle(ws);
    REQUIRE(ws[0]->ref_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1]->ref_tracker.n_triggers_here() == 0);
    REQUIRE(ws[1]->ref_tracker.n_owned() == 0);
}

void remote(int n_workers, int owner_worker, int fulfill_worker,
    int trigger_worker, bool trigger_first, bool always_settle) 
{
    auto ws = workers(n_workers);
    int x = 0;

    auto id = id_on_worker(ws[owner_worker]);
    GlobalRef iv(id);

    auto trigger = [&] () {
        ws[trigger_worker]->get_ref_tracker().add_trigger(iv, {
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

    ws[fulfill_worker]->get_ref_tracker().fulfill(iv, {make_data(1)});

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

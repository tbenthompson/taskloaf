#include "catch.hpp"

#include "taskloaf/worker.hpp"
#include "taskloaf/communicator.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Worker") {
    Worker w;
    int x = 0;
    w.add_task([&] () { x = 1; cur_worker->shutdown(); });
    w.run();
    REQUIRE(x == 1);
}

TEST_CASE("Task collection runs from newest tasks") {
    TaskCollection tc;
    int x = 0;
    tc.add_task([&] () { x = 1; });
    tc.add_task([&] () { x = 2; });
    tc.next()();
    REQUIRE(x == 2);
    tc.next()();
    REQUIRE(tc.size() == 0);
}

TEST_CASE("Task collection steals from oldest tasks") {
    TaskCollection tc; 
    int x = 0;
    tc.add_task([&] () { x = 1; });
    tc.add_task([&] () { x = 2; });
    tc.steal()();
    REQUIRE(x == 1);
    tc.steal()();
    REQUIRE(tc.size() == 0);
}

TEST_CASE("IVar trigger before fulfill") {
    Worker w;
    cur_worker = &w;
    auto ivar = w.new_ivar();
    int x = 0;
    w.add_trigger(ivar, [&] (std::vector<Data>& val) {
        x = val[0].get_as<int>(); 
    });
    Data d{make_safe_void_ptr(10)};
    w.fulfill(ivar, {d});
    REQUIRE(x == 10);
}

TEST_CASE("Trigger after fulfill") {
    Worker w;
    cur_worker = &w;
    auto ivar = w.new_ivar();
    int x = 0;
    Data d{make_safe_void_ptr(10)};
    w.fulfill(ivar, {d});
    w.add_trigger(ivar, [&] (std::vector<Data>& val) {
        x = val[0].get_as<int>(); 
    });
    REQUIRE(x == 10);
}

TEST_CASE("Dec ref deletes") {
    Worker w; 
    cur_worker = &w;
    auto ivarref = w.new_ivar();
    w.dec_ref(ivarref);
    REQUIRE(w.ivars.ivars.size() == 0);
    ivarref.owner.hostname = "";
}

TEST_CASE("Inc ref preserves") {
    Worker w;
    cur_worker = &w;
    {
        auto ivarref = w.new_ivar();
        w.inc_ref(ivarref);
    }
    REQUIRE(w.ivars.ivars.size() == 1);
}

//TODO: Refactor these multi worker tests.
TEST_CASE("Two communicators meet") {
    IVarTracker iv;
    TaskCollection t;
    CAFCommunicator c1;
    CAFCommunicator c2;
    c2.meet(c1.get_addr());
    c1.handle_messages(iv, t);
    REQUIRE(c2.n_friends() == 1);
    REQUIRE(c1.n_friends() == 1);
}

void stealing_test(int n_steals) {
    Worker w1;
    Worker w2;
    int x = 0;
    int n_tasks = 5;
    for (int i = 0; i < n_tasks; i++) {
        w1.add_task([&] () { x = 1; });
    }
    w2.meet(w1.get_addr());
    for (int i = 0; i < n_steals; i++) {
        w2.comm->steal(0);
    }
    REQUIRE(w1.tasks.size() == n_tasks);
    REQUIRE(w2.tasks.size() == 0);
    w1.comm->handle_messages(w1.ivars, w1.tasks);
    REQUIRE(w1.tasks.size() == n_tasks - 1);
    w2.comm->handle_messages(w2.ivars, w2.tasks);
    REQUIRE(w2.tasks.size() == 1);
}

TEST_CASE("Two workers one steal") {
    stealing_test(1);
}

TEST_CASE("Two workers two steals = second does nothing") {
    stealing_test(2);
}

TEST_CASE("Remote reference counting") {
    Worker w1;
    Worker w2;
    {
        cur_worker = &w2;
        auto unused = w2.new_ivar(); (void)unused;
        cur_worker = &w1;
        IVarRef iv(w2.get_addr(), 0);
        cur_worker = &w2;
        w2.comm->handle_messages(w2.ivars, w2.tasks);
        REQUIRE(w2.ivars.ivars.size() == 1);
        cur_worker = &w1;
    }
    cur_worker = &w2;
    w2.comm->handle_messages(w2.ivars, w2.tasks);
    REQUIRE(w2.ivars.ivars.size() == 0);
}

TEST_CASE("Remote reference counting change location") {
    Worker w1;
    Worker w2;
    {
        cur_worker = &w2;
        auto unused = w2.new_ivar(); (void)unused;
        IVarRef iv(w2.get_addr(), 0);
        REQUIRE(w2.ivars.ivars.size() == 1);
        cur_worker = &w1;
    }
    cur_worker = &w2;
    w2.comm->handle_messages(w2.ivars, w2.tasks);
    REQUIRE(w2.ivars.ivars.size() == 0);
}

TEST_CASE("Remote fulfill") {
    Worker w1;
    Worker w2;
    int x = 0;

    cur_worker = &w2;
    auto iv = w2.new_ivar();
    w2.add_trigger(iv, [&] (std::vector<Data>& vals) {
        x = vals[0].get_as<int>(); 
    });

    cur_worker = &w1;
    w1.fulfill(iv, {Data{make_safe_void_ptr(1)}});

    REQUIRE(x == 0);

    cur_worker = &w2;
    w2.comm->handle_messages(w2.ivars, w2.tasks);

    REQUIRE(x == 1);
}

TEST_CASE("Remote add trigger") {
    Worker w1;
    Worker w2;
    int x = 0;

    cur_worker = &w2;
    auto iv = w2.new_ivar();

    cur_worker = &w1;
    w1.add_trigger(iv, [&] (std::vector<Data>& vals) {
        x = vals[0].get_as<int>(); 
    });

    cur_worker = &w2;
    w2.comm->handle_messages(w2.ivars, w2.tasks);

    REQUIRE(x == 0);

    cur_worker = &w2;
    w2.fulfill(iv, {Data{make_safe_void_ptr(1)}});

    REQUIRE(x == 1);
}

#include "catch.hpp"

#include "worker.hpp"
#include "communicator.hpp"

using namespace taskloaf;

TEST_CASE("Worker") {
    Worker w;
    int x = 0;
    w.add_task([&] () { x = 1; });
    w.run();
    REQUIRE(x == 1);
}

TEST_CASE("Task collection get next") {
    TaskCollection tc;
    tc.add_task([] () {});
    tc.next();
    REQUIRE(tc.empty());
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

TEST_CASE("Two workers stealing") {
    Worker w1;
    Worker w2;
    int x = 0;
    w1.add_task([&] () { x = 1; });
    w2.comm->steal();
    // w2.meet(w1.
    REQUIRE(w2.tasks.empty());
    w1.comm->handle_messages(w1.ivars, w1.tasks);
    REQUIRE(!w2.tasks.empty());
}

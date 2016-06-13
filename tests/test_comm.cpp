#include "catch.hpp"

#include "taskloaf/local_comm.hpp"

#include "ownership_tracker.hpp"

#include <cereal/archives/binary.hpp>

using namespace taskloaf;

TEST_CASE("MPMC Queue", "[comm]") {
    moodycamel::ConcurrentQueue<int> q;
    q.enqueue(25);
    int item;
    bool found = q.try_dequeue(item);
    REQUIRE(found);
    REQUIRE(item == 25);
}

void test_comm(comm& a, comm& b) {
    SECTION("Recv") {
        REQUIRE(b.recv().empty());
        int x = 0;
        a.send({1}, make_closure([&] (ignore, ignore) { x = 13; return ignore{}; }));
        b.recv()();
        REQUIRE(x == 13); 
    }

    SECTION("Msg ordering") {
        double x = 4.5;
        a.send({1}, make_closure([&] (ignore,ignore) { x /= 4.5; return ignore{}; }));
        a.send({1}, make_closure([&] (ignore,ignore) { x -= 1.0; return ignore{}; }));
        b.recv()();
        b.recv()();
        REQUIRE(x == 0.0);
    }
}

TEST_CASE("Local comm", "[comm]") {
    auto lcq = std::make_shared<local_comm_queues>(2);
    local_comm a(lcq, 0);
    local_comm b(lcq, 1);
    test_comm(a, b);
}

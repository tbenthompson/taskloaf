#include "catch.hpp"

#include "taskloaf/local_comm.hpp"
#include "taskloaf/fnc.hpp"

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

void test_comm(Comm& a, Comm& b) {
    SECTION("Recv") {
        int x = 0;
        auto nothing_yet = b.recv() == nullptr;
        REQUIRE(nothing_yet);
        int send = 13;
        a.send({1}, make_data(send));
        REQUIRE(b.recv().get_as<int>() == 13); 
    }

    SECTION("Msg ordering") {
        double x = 4.5;
        a.send({1}, make_data(4.5));
        a.send({1}, make_data(1.0));
        auto m1 = b.recv();
        auto m2 = b.recv();
        REQUIRE((x / m1.get_as<double>()) - m2.get_as<double>() == 0.0);
    }

    SECTION("Recv complex") {
        int x = 0;
        auto d = make_data(Function<int(int)>([] (int x) { return 2 * x; })); 
        a.send({1}, std::move(d));
        REQUIRE(b.recv().get_as<Function<int(int)>>()(3) == 6);
    }
}

TEST_CASE("Local comm", "[comm]") {
    auto lcq = std::make_shared<LocalCommQueues>(2);
    LocalComm a(lcq, 0);
    LocalComm b(lcq, 1);
    test_comm(a, b);
}

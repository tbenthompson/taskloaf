#include "catch.hpp"

#include "taskloaf/caf_comm.hpp"
#include "taskloaf/local_comm.hpp"

#include "concurrentqueue.h"

using namespace taskloaf;

TEST_CASE("MPMC Queue", "[local_comm]") {
    moodycamel::ConcurrentQueue<int> q;
    q.enqueue(25);
    int item;
    bool found = q.try_dequeue(item);
    REQUIRE(found);
    REQUIRE(item == 25);
}

TEST_CASE("Local comm", "[local_comm]") {
    auto lcq = std::make_shared<LocalCommQueues>(2);
    LocalComm a(lcq, 0);
    LocalComm b(lcq, 1);

    SECTION("Send") {
        REQUIRE(!b.has_incoming());
        a.send({"", 1}, Msg(0, make_data(13)));
        REQUIRE(b.has_incoming());
        REQUIRE(b.cur_message().data.get_as<int>() == 13);
    }

    SECTION("Recv") {
        int x = 0;
        a.send({"", 1}, Msg(0, make_data(13)));
        b.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
        b.recv();
        REQUIRE(x == 13); 
    }

    SECTION("Recv two") {
        int x = 0;
        a.send({"", 1}, Msg(0, make_data(13)));
        a.send({"", 1}, Msg(0, make_data(23)));
        b.add_handler(0, [&] (Data d) { x += d.get_as<int>(); });
        b.recv();
        b.recv();
        REQUIRE(x == 36); 
    }
}

TEST_CASE("Protocol set up") {
    CAFComm c1;
}

TEST_CASE("Protocol recv") {
    CAFComm c1;
    CAFComm c2;

    c1.send(c2.get_addr(), Msg{0, make_data(10)});

    int x = 0;
    c2.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
    c2.recv();
    REQUIRE(x == 10);
}

TEST_CASE("Two handlers") {
    CAFComm c1;
    CAFComm c2;

    c1.send(c2.get_addr(), Msg{1, make_data(11)});

    int x = 0;
    c2.add_handler(0, [&] (Data d) { (void)d; x = 1; });
    c2.add_handler(1, [&] (Data d) { x = d.get_as<int>(); });
    c2.recv();
    REQUIRE(x == 11);
}

TEST_CASE("Skip unhandled message") {
    CAFComm c1;
    CAFComm c2;
    c1.send(c2.get_addr(), Msg{1, make_data(11)});
    c2.recv();
}

TEST_CASE("Forward") {
    CAFComm c1;
    CAFComm c2;
    c1.send(c2.get_addr(), Msg{0, make_data(11)});

    int x = 0;
    c1.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
    c2.add_handler(0, [&] (Data) { c2.send(c1.get_addr(), c2.cur_message()); });

    c2.recv();
    c1.recv();
    REQUIRE(x == 11);
}

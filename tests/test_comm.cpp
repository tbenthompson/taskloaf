#include "catch.hpp"

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

    SECTION("Recv") {
        int x = 0;
        REQUIRE(!b.has_incoming());
        a.send({"", 1}, Msg(0, make_data(13)));
        REQUIRE(b.has_incoming());
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

    SECTION("Two handlers") {
        a.send(b.get_addr(), Msg{1, make_data(11)});
        int x = 0;
        b.add_handler(0, [&] (Data d) { (void)d; x = 1; });
        b.add_handler(1, [&] (Data d) { x = d.get_as<int>(); });
        b.recv();
        REQUIRE(x == 11);
    }

    SECTION("Skip unhandled") {
        a.send(b.get_addr(), Msg{1, make_data(11)});
        b.recv();
    }

    SECTION("Forward") {
        a.send(b.get_addr(), Msg{0, make_data(11)});

        int x = 0;
        a.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
        b.add_handler(0, [&] (Data) { b.send(a.get_addr(), b.cur_message()); });

        b.recv();
        a.recv();
        REQUIRE(x == 11);
    }
}

#include "catch.hpp"

#include "taskloaf/local_comm.hpp"
#include "taskloaf/serializing_comm.hpp"
#include "taskloaf/fnc.hpp"

#include <concurrentqueue.h>
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

void forwarding_test(Comm& a, Comm& b, bool open) {
    a.send(b.get_addr(), Msg{0, make_data(11)});

    int x = 0;
    a.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
    b.add_handler(0, [&] (Data) {
        if (open) {
            b.cur_message().data.get_as<int>();
        }
        b.send(a.get_addr(), b.cur_message()); 
    });

    b.recv();
    a.recv();
    REQUIRE(x == 11);
}

void test_comm(Comm& a, Comm& b) {
    SECTION("Recv") {
        int x = 0;
        REQUIRE(!b.has_incoming());
        a.send({"", 1}, Msg(0, make_data(13)));
        REQUIRE(b.has_incoming());
        b.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
        b.recv();
        REQUIRE(x == 13); 
    }

    SECTION("Msg ordering") {
        double x = 4.5;
        b.add_handler(0, [&] (Data d) { x /= d.get_as<double>(); });
        b.add_handler(1, [&] (Data d) { x -= d.get_as<double>(); });
        a.send({"", 1}, Msg(0, make_data(4.5)));
        a.send({"", 1}, Msg(1, make_data(1.0)));
        b.recv();
        b.recv();
        REQUIRE(x == 0.0);
    }

    SECTION("Recv complex") {
        int x = 0;
        auto d = make_data(Function<int(int)>([] (int x) { return 2 * x; })); 
        a.send({"", 1}, Msg(0, std::move(d)));
        b.add_handler(0, [&] (Data d) { x = d.get_as<Function<int(int)>>()(3); });
        b.recv();
        REQUIRE(x == 6);
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
        forwarding_test(a, b, true);
    }

    SECTION("Forward unopened message") {
        forwarding_test(a, b, false);
    }
}

TEST_CASE("Local comm", "[comm]") {
    auto lcq = std::make_shared<LocalCommQueues>(2);
    LocalComm a(lcq, 0);
    LocalComm b(lcq, 1);
    test_comm(a, b);
}

TEST_CASE("Serializing Comm", "[comm]") {
    auto lcq = std::make_shared<LocalCommQueues>(2);
    SerializingComm a(std::make_unique<LocalComm>(lcq, 0));
    SerializingComm b(std::make_unique<LocalComm>(lcq, 1));
    test_comm(a, b);
}

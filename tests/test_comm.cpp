#include "catch.hpp"

#include "taskloaf/local_comm.hpp"
#include "taskloaf/serializing_comm.hpp"
#include "taskloaf/fnc.hpp"

#include "delete_tracker.hpp"

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
        a.send(b.get_addr(), Msg{0, make_data(11)});

        int x = 0;
        a.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
        b.add_handler(0, [&] (Data) {
            b.send(a.get_addr(), b.cur_message()); 
        });

        b.recv();
        a.recv();
        REQUIRE(x == 11);
    }

    SECTION("Delete msg after handling") {
        a.send(b.get_addr(), Msg{0, make_data(DeleteTracker())});
        DeleteTracker::get_deletes() = 0;
        b.add_handler(0, [&] (Data) {});
        b.recv();
        REQUIRE(DeleteTracker::get_deletes() == 1);
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

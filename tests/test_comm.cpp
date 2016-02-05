#include "catch.hpp"

#include "taskloaf/caf_comm.hpp"

using namespace taskloaf;

TEST_CASE("Protocol set up") {
    CAFComm p1;
}

TEST_CASE("Protocol recv") {
    CAFComm p1;
    CAFComm p2;

    p1.send(p2.get_addr(), Msg{0, make_data(10)});

    int x = 0;
    p2.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
    p2.recv();
    REQUIRE(x == 10);
}

TEST_CASE("Two handlers") {
    CAFComm p1;
    CAFComm p2;

    p1.send(p2.get_addr(), Msg{1, make_data(11)});

    int x = 0;
    p2.add_handler(0, [&] (Data d) { (void)d; x = 1; });
    p2.add_handler(1, [&] (Data d) { x = d.get_as<int>(); });
    p2.recv();
    REQUIRE(x == 11);
}

TEST_CASE("Skip unhandled message") {
    CAFComm p1;
    CAFComm p2;
    p1.send(p2.get_addr(), Msg{1, make_data(11)});
    p2.recv();
}

TEST_CASE("Forward") {
    CAFComm p1;
    CAFComm p2;
    p1.send(p2.get_addr(), Msg{0, make_data(11)});

    int x = 0;
    p1.add_handler(0, [&] (Data d) { x = d.get_as<int>(); });
    p2.add_handler(0, [&] (Data) { p2.forward(p1.get_addr()); });

    p2.recv();
    p1.recv();
    REQUIRE(x == 11);
}

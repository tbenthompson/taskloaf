#include "catch.hpp"

#include "taskloaf/comm.hpp"

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

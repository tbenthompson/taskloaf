#include "catch.hpp"

#include "taskloaf/ring.hpp"
#include "taskloaf/caf_comm.hpp"

using namespace taskloaf; 

TEST_CASE("Local", "[ring]") {
    CAFComm c;
    Ring r(c, 1);
}

TEST_CASE("Two nodes meet", "[ring]") {
    CAFComm c1;
    Ring r1(c1, 1);
    CAFComm c2;
    Ring r2(c2, c1.get_addr(), 1);
    c1.recv();
    c2.recv();
    REQUIRE(r1.ring_size() == 2);
    REQUIRE(r2.ring_size() == 2);

    auto id = r1.get_locs()[0];
    id.secondhalf += 1;
    REQUIRE(r1.get_owner(id) == c2.get_addr());
    REQUIRE(r2.get_owner(id) == c2.get_addr());
}

TEST_CASE("Two gossipers", "[ring]") {
    auto ids = new_ids(2);
    CAFComm c1;
    Ring r1(c1, 1);
    CAFComm c2;
    Ring r2(c2, 1);
    r1.introduce(c2.get_addr());
    c2.recv();
    c1.recv();
    REQUIRE(r1.ring_size() == 2);
    REQUIRE(r2.ring_size() == 2);
}

TEST_CASE("Three gossipers", "[ring]") {
    CAFComm c1;
    Ring r1(c1, 1);
    CAFComm c2;
    Ring r2(c2, 1);
    CAFComm c3;
    Ring r3(c3, 1);
    r1.introduce(c2.get_addr());
    r2.introduce(c3.get_addr());
    c3.recv();
    c1.recv();
    c2.recv();
    REQUIRE(r1.ring_size() == 3);
    REQUIRE(r2.ring_size() == 3);
    REQUIRE(r3.ring_size() == 3);
}

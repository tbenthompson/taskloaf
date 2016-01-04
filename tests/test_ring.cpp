#include "catch.hpp"

#include "taskloaf/ring.hpp"
#include "taskloaf/caf_comm.hpp"

#include <iostream>

using namespace taskloaf; 

TEST_CASE("Local", "[ring]") {
    CAFComm c;
    Ring<int> r(c, 1);
    auto id = new_id();
    r.set(id, 15);
    bool correct = false;
    r.get(id, [&] (int val) {
        if (val == 15) {
            correct = true;
        }
    });
    REQUIRE(correct);
}

TEST_CASE("Two nodes meet", "[ring]") {
    CAFComm c1;
    Ring<int> r1(c1, 1);
    CAFComm c2;
    Ring<int> r2(c2, r1.get_address(), 1);
    r1.handle_messages();
    r2.handle_messages();
    REQUIRE(r1.ring_size() == 2);
    REQUIRE(r2.ring_size() == 2);
}

TEST_CASE("Two nodes share", "[ring]") {
    CAFComm c1;
    Ring<int> r1(c1, 1);
    CAFComm c2;
    Ring<int> r2(c2, r1.get_address(), 1);
    r1.handle_messages();
    r2.handle_messages();

    auto id = r1.get_locs()[0];
    id.secondhalf += 1;
    REQUIRE(r1.get_owner(id) == c2.get_addr());

    r1.set(id, 10);
    r2.handle_messages();
    bool correct = false;
    r2.get(id, [&] (int val) {
        if (val == 10) {
            correct = true;
        }
    });
    REQUIRE(correct);

    correct = false;
    r1.get(id, [&] (int val) {
        if (val == 10) {
            correct = true;
        }
    });
    REQUIRE(!correct);
    r2.handle_messages();
    r1.handle_messages();
    REQUIRE(correct);
}

TEST_CASE("Make gossiper", "[ring]") {
    CAFComm c;
    Gossiper g(c, new_ids(1));
}

TEST_CASE("Two gossiper meet", "[ring]") {
    auto ids = new_ids(2);
    CAFComm c1;
    Gossiper g1(c1, {ids[0]});
    CAFComm c2;
    Gossiper g2(c2, {ids[1]});
    g1.introduce(g2.my_state.addr);
    g2.handle_messages();
    g1.handle_messages();
    REQUIRE(g1.friends.size() == 1);
    REQUIRE(g2.friends.size() == 1);
    REQUIRE(g1.sorted_ids.size() == 2);
    REQUIRE(g2.sorted_ids.size() == 2);
}

TEST_CASE("Three gossiper meet", "[ring]") {
    CAFComm c1;
    Gossiper g1(c1, new_ids(1));
    CAFComm c2;
    Gossiper g2(c2, new_ids(1));
    CAFComm c3;
    Gossiper g3(c3, new_ids(1));
    g1.introduce(g2.my_state.addr);
    g2.introduce(g3.my_state.addr);
    g3.handle_messages();
    g1.handle_messages();
    g2.handle_messages();
    REQUIRE(g1.friends.size() == 2);
    REQUIRE(g2.friends.size() == 2);
    REQUIRE(g3.friends.size() == 2);
}

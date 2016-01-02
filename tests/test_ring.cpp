#include "catch.hpp"

#include "taskloaf/ring.hpp"

#include <iostream>

using namespace taskloaf; 

TEST_CASE("Local", "[ring]") {
    Ring<int> r(1);
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
    Ring<int> r1(1);
    Ring<int> r2(r1.get_address(), 1);
    r1.handle_messages();
    r2.handle_messages();
    REQUIRE(r1.ring_size() == 2);
    REQUIRE(r2.ring_size() == 2);
}

TEST_CASE("Two nodes share", "[ring]") {
    Ring<int> r1(1);
    Ring<int> r2(r1.get_address(), 1);
    r1.handle_messages();
    r2.handle_messages();

    // set a location virtually guaranteed to be on r1 from r2
    // auto loc = r1.get_locs()[0];
    // loc.secondhalf += 1;
    // r2[loc] = 15;
    // r1.handle_messages();
    // // r2.
}

TEST_CASE("Make gossiper", "[ring]") {
    Gossip g(new_ids(1));
    REQUIRE(g.comm != nullptr);
}

TEST_CASE("Two gossiper meet", "[ring]") {
    Gossip g1(new_ids(1));
    Gossip g2(new_ids(1));
    g1.introduce(g2.my_state.addr);
    g2.handle_messages();
    g1.handle_messages();
    REQUIRE(g1.friends.size() == 1);
    REQUIRE(g2.friends.size() == 1);
}

TEST_CASE("Three gossiper meet", "[ring]") {
    Gossip g1(new_ids(1));
    Gossip g2(new_ids(1));
    Gossip g3(new_ids(1));
    g1.introduce(g2.my_state.addr);
    g2.introduce(g3.my_state.addr);
    g3.handle_messages();
    g2.handle_messages();
    g1.handle_messages();
    REQUIRE(g1.friends.size() == 2);
    REQUIRE(g2.friends.size() == 2);
    REQUIRE(g3.friends.size() == 2);
}

#include "catch.hpp"

#include "taskloaf/ring.hpp"
#include "taskloaf/caf_comm.hpp"
#include "taskloaf/protocol.hpp"

#include <iostream>

using namespace taskloaf; 

struct TestRing {
    CAFComm c;
    Ring r;
    TestRing(int n_locs):
        r(c, n_locs)
    {}
};

TEST_CASE("Local", "[ring]") {
    TestRing(1);
}

TEST_CASE("Two nodes meet", "[ring]") {
    TestRing tr1(1);
    TestRing tr2(1);
    tr2.r.introduce(tr1.c.get_addr());
    tr1.c.recv();
    tr2.c.recv();
    REQUIRE(tr1.r.ring_size() == 2);
    REQUIRE(tr2.r.ring_size() == 2);

    auto id = tr1.r.get_locs()[0];
    id.secondhalf -= 1;
    REQUIRE(tr1.r.get_owner(id) == tr2.c.get_addr());
    REQUIRE(tr2.r.get_owner(id) == tr2.c.get_addr());
}

TEST_CASE("Three gossipers", "[ring]") {
    TestRing tr1(1);
    TestRing tr2(1);
    TestRing tr3(1);
    tr1.r.introduce(tr2.c.get_addr());
    tr2.r.introduce(tr3.c.get_addr());
    for (int i = 0; i < 10; i++) {
        tr3.c.recv();
        tr1.c.recv();
        tr2.c.recv();
    }
    REQUIRE(tr1.r.ring_size() == 3);
    REQUIRE(tr2.r.ring_size() == 3);
    REQUIRE(tr3.r.ring_size() == 3);
}

TEST_CASE("Ownership transfer", "[ring]") {
    TestRing tr1(1);
    TestRing tr2(1);
    tr2.r.introduce(tr1.c.get_addr());
    bool ran = false;
    tr1.c.add_handler(to_underlying(Protocol::InitiateTransfer), [&] (Data d) {
        auto& p = d.get_as<std::tuple<ID,ID,Address>>();
        ran = true; 
        REQUIRE(std::get<0>(p) == tr2.r.get_locs()[0]);
        REQUIRE(std::get<1>(p) == tr1.r.get_locs()[0]);
        REQUIRE(std::get<2>(p) == tr2.c.get_addr());
    });
    tr1.c.recv();
    tr2.c.recv();
    REQUIRE(ran);
}

TEST_CASE("Transfer interval", "[ring]") {
    TestRing tr1(1);
    auto id = new_id();
    auto ts = tr1.r.compute_transfers({id});
    auto loc = tr1.r.get_locs()[0];
    REQUIRE(ts[0].first == id);
    REQUIRE(ts[0].second == loc);
}

TEST_CASE("In interval", "[ring]") {
    REQUIRE(!in_interval({10, 0}, {1, 0}, {5, 0}));
    REQUIRE(in_interval({0, 0}, {10, 0}, {5, 0}));
    REQUIRE(!in_interval({0, 0}, {10, 0}, {11, 0}));
    REQUIRE(in_interval({1000, 0}, {10, 0}, {5, 0}));
    REQUIRE(in_interval({3, 0}, {1, 0}, {5, 0}));
}

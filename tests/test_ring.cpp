#include "catch.hpp"

#include "taskloaf/ring.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/protocol.hpp"

#include <iostream>

using namespace taskloaf; 

struct TestRing {
    std::vector<LocalComm> cs;
    std::vector<Ring> rs;
    TestRing(size_t n_members, size_t n_locs_per_member)
    {
        auto qs = std::make_shared<LocalCommQueues>(n_members);
        for (size_t i = 0; i < n_members; i++) {
            cs.emplace_back(qs, i);
        }

        for (size_t i = 0; i < n_members; i++) {
            rs.push_back(Ring(cs[i], n_locs_per_member));
        }
    }
};

TEST_CASE("Two nodes meet", "[ring]") {
    TestRing tr(2, 1);
    tr.rs[1].introduce(tr.cs[0].get_addr());
    tr.cs[0].recv();
    tr.cs[1].recv();
    REQUIRE(tr.rs[0].ring_size() == 2);
    REQUIRE(tr.rs[1].ring_size() == 2);

    auto id = tr.rs[0].get_locs()[0];
    id.secondhalf -= 1;
    REQUIRE(tr.rs[0].get_owner(id) == tr.cs[1].get_addr());
    REQUIRE(tr.rs[1].get_owner(id) == tr.cs[1].get_addr());
}

TEST_CASE("Three gossipers", "[ring]") {
    TestRing tr(3, 1);
    tr.rs[0].introduce(tr.cs[1].get_addr());
    tr.rs[1].introduce(tr.cs[2].get_addr());
    for (int i = 0; i < 10; i++) {
        tr.cs[2].recv();
        tr.cs[0].recv();
        tr.cs[1].recv();
    }
    REQUIRE(tr.rs[0].ring_size() == 3);
    REQUIRE(tr.rs[1].ring_size() == 3);
    REQUIRE(tr.rs[2].ring_size() == 3);
}

TEST_CASE("Ownership transfer", "[ring]") {
    TestRing tr(2, 1);
    tr.rs[1].introduce(tr.cs[0].get_addr());
    bool ran = false;
    tr.cs[0].add_handler(to_underlying(Protocol::InitiateTransfer), [&] (Data d) {
        auto& p = d.get_as<std::tuple<ID,ID,Address>>();
        ran = true; 
        REQUIRE(std::get<0>(p) == tr.rs[1].get_locs()[0]);
        REQUIRE(std::get<1>(p) == tr.rs[0].get_locs()[0]);
        REQUIRE(std::get<2>(p) == tr.cs[1].get_addr());
    });
    tr.cs[0].recv();
    tr.cs[1].recv();
    REQUIRE(ran);
}

TEST_CASE("Transfer interval", "[ring]") {
    TestRing tr(1, 1);
    auto id = new_id();
    auto ts = tr.rs[0].compute_transfers({id});
    auto loc = tr.rs[0].get_locs()[0];
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

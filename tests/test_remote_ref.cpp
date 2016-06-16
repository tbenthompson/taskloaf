#include "catch.hpp"

#include "taskloaf/remote_ref.hpp"

#include "ownership_tracker.hpp"

using namespace taskloaf;

struct OwnershipTrackerWithRef {
    OwnershipTracker o;
    intrusive_ref_count ref_count;
};

using tracker_ref = remote_ref<OwnershipTrackerWithRef>;

TEST_CASE("Create remote ref") {
    OwnershipTracker::reset();
    cur_addr = address{0};
    tracker_ref rr;
    REQUIRE(rr.owner == address{0});
    REQUIRE(OwnershipTracker::constructs() == 1);
}

TEST_CASE("Delete underlying") {
    OwnershipTracker::reset();
    cur_addr = address{0};

    SECTION("Just one") {
        tracker_ref rr;
        {
            tracker_ref rr;
            REQUIRE(OwnershipTracker::constructs() == 2);
        }
        REQUIRE(OwnershipTracker::deletes() == 1);
    }

    SECTION("All") {
        {
            tracker_ref rr;
            tracker_ref rr2;
            REQUIRE(OwnershipTracker::constructs() == 2);
            REQUIRE(OwnershipTracker::deletes() == 0);
        }
        REQUIRE(OwnershipTracker::deletes() == 2);
    }
}

TEST_CASE("Copy ref") {
    cur_addr = address{0};
    tracker_ref rr;
    auto rr2 = rr;
    REQUIRE(rr.children == 1);
    REQUIRE(rr2.hdl == rr.hdl);
}

TEST_CASE("Move ref") {
    cur_addr = address{0};
    tracker_ref rr;
    auto rr2 = std::move(rr);
    REQUIRE(rr.hdl == nullptr);
}

TEST_CASE("Copy empty") {
    tracker_ref rr;
    auto rr2 = std::move(rr);
    auto rr3 = rr;
}

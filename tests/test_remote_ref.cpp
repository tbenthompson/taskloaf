#include "doctest.h"

#include "taskloaf/remote_ref.hpp"

#include "ownership_tracker.hpp"
#include "fake_worker.hpp"

namespace doctest {//TODO: Can be removed with doctest 1.1
template <>
struct StringMaker<std::nullptr_t> {
    static String convert(std::nullptr_t) { return "nullptr"; }
};
}

using namespace taskloaf;

struct OwnershipTrackerWithRef {
    OwnershipTracker o;
    intrusive_ref_count ref_count;
};

using tracker_ref = remote_ref<OwnershipTrackerWithRef>;

#define SET_WORKER\
    fake_worker w{0};\
    do {\
        cur_worker = &w;\
    } while (0)

TEST_CASE("Create remote ref") {
    OwnershipTracker::reset();
    SET_WORKER;
    tracker_ref rr;
    REQUIRE(rr.owner == address{0});
    REQUIRE(OwnershipTracker::constructs() == 1);
}

TEST_CASE("Delete underlying") {
    OwnershipTracker::reset();
    SET_WORKER;

    SUBCASE("Just one") {
        tracker_ref rr;
        {
            tracker_ref rr;
            REQUIRE(OwnershipTracker::constructs() == 2);
        }
        REQUIRE(OwnershipTracker::deletes() == 1);
    }

    SUBCASE("All") {
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
    SET_WORKER;
    tracker_ref rr;
    auto rr2 = rr;
    REQUIRE(int(rr.internal.children) == 1);
    REQUIRE(rr2.internal.hdl == rr.internal.hdl);
}

TEST_CASE("Move ref") {
    SET_WORKER;
    tracker_ref rr;
    auto rr2 = std::move(rr);
    REQUIRE(rr.internal.hdl == nullptr);
}

TEST_CASE("Copy empty") {
    SET_WORKER;
    tracker_ref rr;
    auto rr2 = std::move(rr);
    auto rr3 = rr;
}

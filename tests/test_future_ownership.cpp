#include "catch.hpp"

#include "taskloaf.hpp"
#include "ownership_tracker.hpp"

/* Rvalue-ness or lvalue-ness of a future should propagate to the contained
 * value, with one exception: the object passed to the continuation function 
 * provided to .then is always an lvalue. To avoid copying, the user can 
 * decide when it is acceptable to move from that lvalue. 
 */

using namespace taskloaf;

TEST_CASE("Local no copy with rvalue", "[future_ownership]") {
    OwnershipTracker::reset();

    SECTION("Ready") {
        auto f = ready(OwnershipTracker());
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Then") {
        auto f = async([] { return OwnershipTracker(); })
            .then([] (OwnershipTracker&) {});
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("When all") {
        auto f = when_all(
            async([] { return OwnershipTracker(); }),
            async([] { return OwnershipTracker(); })
        );
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Unwrap") {
        auto f = async(
            [] { return async([] { return OwnershipTracker(); }); }
        ).unwrap();
        REQUIRE(OwnershipTracker::copies() == 0);
    }
}

TEST_CASE("Global", "[future_ownership]") {
    OwnershipTracker::reset();
    launch_local(1, [&] () {
        auto f = when_all(
            asyncd([] { return OwnershipTracker(); }),
            asyncd([] { return OwnershipTracker(); })
        );
        REQUIRE(OwnershipTracker::copies() == 0);
        shutdown();
    });
}

TEST_CASE("Copy with lvalue", "[future_ownership]") {
    OwnershipTracker::reset();

    SECTION("Ready") {
        OwnershipTracker o{};
        auto f = ready(o);
        REQUIRE(OwnershipTracker::copies() == 1);
    }

    SECTION("Then") {
        auto f1 = async([] { return OwnershipTracker(); });
        auto f2 = f1.then([] (const OwnershipTracker& o) { return o; });
        REQUIRE(OwnershipTracker::copies() == 1);
    }

    SECTION("When all") {
        auto f1 = async([] { return OwnershipTracker(); });
        auto f2 = async([] { return OwnershipTracker(); });
        auto f = when_all(f1, f2);
        REQUIRE(OwnershipTracker::copies() == 2);
    }

    SECTION("Unwrap") {
        launch_local(1, [&] () {
            auto f = async([] { return async([] {}); });
            auto f2 = f.unwrap();
            // The interior future should be copied from f.get() to f2 and
            // result in a local to global promotion.
            REQUIRE(f2.is_global());
            shutdown();
        });
    }
}

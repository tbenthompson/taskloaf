#include "catch.hpp"

#include "taskloaf.hpp"
#include "ownership_tracker.hpp"

/* Rvalue-ness or lvalue-ness of a future should propagate to the contained
 * value. For example, 
 * 
 * valid:
 * async([] { return Object(); }).then([] (Object) {}) 
 * async([] { return Object(); }).then([] (Object&&) {}) 
 * async([] { return Object(); }).then([] (const Object&) {}) 
 *
 * invalid:
 * async([] { return Object(); }).then([] (Object&) {}) 
 *
 * because the result of async is an rvalue and thus cannot be bound to an
 * lvalue reference. 
 *
 * Contrast with:
 * auto f = async([] { return Object(); })
 * valid:
 * f.then([] (Object) {}) 
 * f.then([] (Object&) {}) 
 * f.then([] (const Object&) {}) 
 * std::move(f).then([] (Object&&) {})
 *
 * invalid:
 * f.then([] (Object&&) {})
 *
 * because f is an lvalue and thus cannot be bound to an rvalue reference.
 */

using namespace taskloaf;

TEST_CASE("No copy with rvalue", "[future_ownership]") {
    OwnershipTracker::reset();

    SECTION("Ready") {
        auto f = ready(OwnershipTracker());
        REQUIRE(OwnershipTracker::copies() == 0);
    }

    SECTION("Then") {
        auto f = async([] { return OwnershipTracker(); })
            .then([] (OwnershipTracker) {});
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

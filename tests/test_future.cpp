#include "catch.hpp"

#include "taskloaf/future.hpp"
#include "taskloaf/launcher.hpp"

#include <cereal/types/string.hpp>

using namespace taskloaf;

size_t refs(const remote_ref& rr) {
    return cur_ivar_db->refs(rr.hdl.h);
}

TEST_CASE("Create ivar") {
    auto ctx = launch_local(1);
    cur_addr = address{0};
    remote_ref rr;
    REQUIRE(rr.owner == address{0});
    REQUIRE(refs(rr) == 1);
}

TEST_CASE("Delete ivar") {
    auto ctx = launch_local(1);
    cur_addr = address{0};

    SECTION("Just one") {
        remote_ref rr;
        {
            remote_ref rr;
            REQUIRE(cur_ivar_db->size() == 2);
        }
        REQUIRE(cur_ivar_db->size() == 1);
    }

    SECTION("All") {
        {
            remote_ref rr;
            remote_ref rr2;
            REQUIRE(cur_ivar_db->size() == 2);
        }
        REQUIRE(cur_ivar_db->size() == 0);
    }
}

TEST_CASE("Delete ref") {
    auto ctx = launch_local(1);
    remote_ref rr;
    {
        auto rr2 = rr;
        REQUIRE(refs(rr2) == 2);
    }
    REQUIRE(refs(rr) == 1);
}

TEST_CASE("Copy ivar") {
    auto ctx = launch_local(1);
    remote_ref rr;
    auto rr2 = rr;
    REQUIRE(refs(rr2) == 2);
    REQUIRE(rr2.hdl.h == rr.hdl.h);
}

TEST_CASE("Move ivar") {
    auto ctx = launch_local(1);
    remote_ref rr;
    auto rr2 = std::move(rr);
    REQUIRE(refs(rr2) == 1);
    REQUIRE(rr.is_null());
}

TEST_CASE("Ready") {
    auto ctx = launch_local(1);
    auto f = ready(10);
    int x = f.get();
    REQUIRE(x == 10);
}

TEST_CASE("Then") {
    auto ctx = launch_local(1);
    auto f = ready(10);
    int x = f.then([] (ignore&, int x) { return x * 2; }).get();
    REQUIRE(x == 20);
}

TEST_CASE("Async") {
    auto ctx = launch_local(1);
    int result = async([] (ignore&, ignore&) { return 30; }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Unwrap") {
    auto ctx = launch_local(1);
    std::string x = async([] (ignore&, ignore&) {
        return async([] (ignore&, ignore&) {
            return std::string("HI");
        });
    }).unwrap().get();
    REQUIRE(x == "HI");
}

TEST_CASE("Async elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = async(1, [] (_,_) {
        REQUIRE(cur_addr == address{1});
        return 30;
    }).get();
    REQUIRE(result == 30);
}

TEST_CASE("Then elsewhere") {
    auto ctx = launch_local(2);
    REQUIRE(cur_addr == address{0});
    int result = ready(2).then(1, [] (_,int x) {
        bool not_0 = cur_addr == address{1};
        REQUIRE(not_0);
        return x + 7; 
    }).get();
    REQUIRE(result == 9);
}

TEST_CASE("Unwrap from elsewhere") {
    auto ctx = launch_local(2);
    int x = async(0, [] (_,_) {
        return ready(1); 
    }).unwrap().get();
    REQUIRE(x == 1);
}

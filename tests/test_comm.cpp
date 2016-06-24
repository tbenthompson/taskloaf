#include "doctest.h"

#include "taskloaf/local_comm.hpp"

#include "ownership_tracker.hpp"
#include "fake_worker.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

using namespace taskloaf;

TEST_CASE("MPMC Queue") {
    moodycamel::ConcurrentQueue<int> q;
    q.enqueue(25);
    int item;
    bool found = q.try_dequeue(item);
    REQUIRE(found);
    REQUIRE(item == 25);
}

void test_comm(comm& a, comm& b) {
    SUBCASE("Recv") {
        REQUIRE(b.recv().empty());
        int x = 0;
        a.send({1}, closure([&] (_, _) { x = 13; return _{}; }));
        b.recv()();
        REQUIRE(x == 13); 
    }

    SUBCASE("Msg ordering") {
        double x = 4.5;
        a.send({1}, closure([&] (_,_) { x /= 4.5; return _{}; }));
        a.send({1}, closure([&] (_,_) { x -= 1.0; return _{}; }));
        b.recv()();
        b.recv()();
        REQUIRE(x == 0.0);
    }

    SUBCASE("Deep copy") {
        fake_worker w(0);
        cur_worker = &w;
        auto d = data(std::vector<double>{});
        a.send({1}, closure([] (_,_) { return _{}; }, d)); 
        w.addr = {1};
        auto cl = b.recv();
        // std::cout << cl.d.ptr << " " << d.ptr << std::endl;
        // std::cout <<  << std::endl;
    }
}

TEST_CASE("Local comm") {
    local_comm_queues lcq(2);
    local_comm a(lcq, 0);
    local_comm b(lcq, 1);
    test_comm(a, b);
}

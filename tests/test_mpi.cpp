#include "doctest.h"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace taskloaf;

void test_send(closure f) {
    mpi_comm c;
    if (mpi_rank() == 0) {
        c.send({1}, f);
    } else {
        bool stop = false;
        while (!stop) {
            auto d = c.recv();
            if (d.empty()) {
                continue;
            }
            d();
            stop = true;
        }
    }
}

void test_send_simple() {
    test_send(closure(
        [] (int v, ignore&) { REQUIRE(v == 10); return ignore{};},
        10
    ));
}

void test_send_data() {
    test_send(closure(
        [] (std::string s, ignore&) { REQUIRE(s == "HI"); return ignore{};},
        std::string("HI")
    ));
}

void test_send_closure_lambda() {
    int b = 3;
    test_send(closure(
        [] (closure& f, ignore&) {
            int x = f(data(3));
            REQUIRE(x == 9);
            return ignore{};
        },
        closure([=] (ignore&, int a) { return a * b; })
    ));
}

void test_send_closure_functor() {
    auto f = get_serializable_functor();

    test_send(closure(
        [] (closure& f, ignore&) { 
            int x = f(data(5));
            REQUIRE(x == 120);
            return ignore{};
        },
        std::move(f)
    ));
}

TEST_CASE("MPI") {
    test_send_simple();
    test_send_data();
    test_send_closure_lambda();
    test_send_closure_functor();
}

TEST_CASE("MPI Remote task") {
    auto ctx = launch_mpi();
    
    std::string s("HI");
    if (cur_addr == address{0}) {

        cur_worker->add_task({1}, closure([&] (std::string s,_) {

            REQUIRE(cur_addr == address{1});
            REQUIRE(s == "HI");

            cur_worker->add_task({0}, closure([&] (std::string s,_) {

                REQUIRE(cur_addr == address{0});
                REQUIRE(s == "HI");

                cur_worker->shutdown();
                return _{};

            }, s));
            return _{};

        }, s));
        cur_worker->run();
    }
}

TEST_CASE("MPI Remote async") {
    auto ctx = launch_mpi();

    if (cur_addr.id == 0) {
        REQUIRE(cur_addr == address{0});
        int x = ut_async(1, [] (_,_) {
            REQUIRE(cur_addr == address{1});
            return 13; 
        }).get();
        REQUIRE(x == 13);
    }
}

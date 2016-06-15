#include "catch.hpp"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf/closure.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace taskloaf;

void test_send(closure f) {
    mpi_comm c;
    if (mpi_rank(c) == 0) {
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
    MPI_Init(nullptr, nullptr);

    test_send_simple();
    test_send_data();
    test_send_closure_lambda();
    test_send_closure_functor();

    std::cout << "MPI Tests passed" << std::endl;

    MPI_Finalize();
}

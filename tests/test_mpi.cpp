#include "catch.hpp"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf/closure.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace taskloaf;

void test_send(closure f) {
    MPIComm c;
    if (mpi_rank(c) == 0) {
        c.send({1}, f);
    } else {
        bool stop = false;
        while (!stop) {
            auto d = c.recv();
            if (d == nullptr) {
                continue;
            }
            d();
            stop = true;
        }
    }
}

void test_send_simple() {
    test_send(closure(
        [] (int v, Data&) { REQUIRE(v == 10); return Data{};},
        10
    ));
}

void test_send_data() {
    test_send(closure(
        [] (std::string s, Data&) { REQUIRE(s == "HI"); return Data{};},
        std::string("HI")
    ));
}

void test_send_closure_lambda() {
    int b = 3;
    test_send(closure(
        [] (closure& f, Data&) {
            int x = f(ensure_data(3));
            REQUIRE(x == 9);
            return Data{};
        },
        closure([=] (Data&, int a) { return a * b; })
    ));
}

void test_send_closure_functor() {
    auto f = get_serializable_functor();

    test_send(closure(
        [] (closure& f, Data&) { 
            int x = f(ensure_data(5));
            REQUIRE(x == 120);
            return Data{};
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

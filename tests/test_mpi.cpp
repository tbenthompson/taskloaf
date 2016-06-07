#include "catch.hpp"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf/closure.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

using namespace taskloaf;

void test_send(Closure<void()> f) {
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
    test_send(Closure<void()>(
        [] (int v) { REQUIRE(v == 10); },
        10
    ));
}

void test_send_data() {
    test_send(Closure<void()>(
        [] (std::string s) { REQUIRE(s == "HI"); },
        std::string("HI")
    ));
}

void test_send_closure_lambda() {
    int b = 3;
    test_send(Closure<void()>(
        [] (Closure<int(int)>& f) { REQUIRE(f(3) == 9); },
        Closure<int(int)>([=] (int a) { return a * b; })
    ));
}

void test_send_closure_functor() {
    auto f = get_serializable_functor();

    test_send(Closure<void()>(
        [] (Closure<int(int)>& f) { REQUIRE(f(5) == 120); },
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

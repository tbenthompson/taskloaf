#include "catch.hpp"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf/serializing_comm.hpp"
#include "taskloaf/future.hpp"
#include "taskloaf/fnc.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>

using namespace taskloaf;

template <typename T, typename F>
void test_send(T&& v, F&& f) {
    SerializingComm c(std::make_unique<MPIComm>());
    if (mpi_rank(c) == 0) {
        c.send({1}, Msg(0, make_data(std::forward<T>(v))));
        c.send({1}, Msg(1, make_data(0)));
    } else {
        bool stop = false;
        bool handler_ran = false;
        c.add_handler(0, [&] (Data d) {
            handler_ran = true;
            auto& val = d.get_as<typename std::decay<T>::type>();
            f(c, val);
        });
        c.add_handler(1, [&] (Data) {
            stop = true;
        });
        while (!stop) {
            c.recv();
        }
        REQUIRE(handler_ran);
    }
}

void test_send_simple() {
    test_send(10, [] (Comm&, int v) {
        REQUIRE(v == 10); 
    });
}

void test_send_fnc() {
    int b = 3;
    Function<int(int)> f([=] (int a) { return a * b; });

    test_send(f, [] (Comm&, Function<int(int)> f) {
        REQUIRE(f(3) == 9);
    });
}

void test_send_nested_fnc() {
    Function<int(int)> f([] (int a) { return a * 2; });
    Function<int(int)> f2([=] (int a) { return f(a) * 3; });

    test_send(f2, [] (Comm&, Function<int(int)> f) {
        REQUIRE(f(3) == 18);
    });
}

void test_send_data() {
    auto d = make_data(std::string("HI"));
    test_send(d, [] (Comm&, Data d) {
        REQUIRE(d.get_as<std::string>() == "HI");
    });
}

void test_send_closure() {
    auto f = get_serializable_functor();

    test_send(std::move(f), [] (Comm&, decltype(f)& f) {
        REQUIRE(f(5) == 120);
    });
}

TEST_CASE("MPI") {
    MPI_Init(nullptr, nullptr);

    test_send_simple();
    test_send_fnc();
    test_send_nested_fnc();
    test_send_data();

    std::cout << "MPI Tests passed" << std::endl;

    MPI_Finalize();
}

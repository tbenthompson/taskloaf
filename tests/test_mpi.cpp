#include "taskloaf/mpi_comm.hpp"
#include "taskloaf/serializing_comm.hpp"
#include "taskloaf/future.hpp"
#include "taskloaf/fnc.hpp"

#include <cereal/types/vector.hpp>

using namespace taskloaf;

template <typename T, typename F>
void test_send(T&& v, F&& f) {
    SerializingComm c(std::make_unique<MPIComm>());
    if (mpi_rank(c) == 0) {
        c.send({"", 1}, Msg(0, make_data(std::forward<T>(v))));
        c.send({"", 1}, Msg(1, make_data(0)));
    } else {
        bool stop = false;
        c.add_handler(0, [&] (Data d) {
            auto val = d.get_as<typename std::decay<T>::type>();
            f(c, val);
        });
        c.add_handler(1, [&] (Data) {
            stop = true;
        });
        while (!stop) {
            c.recv();
        }
    }
}

void test_send_simple() {
    test_send(10, [] (Comm&, auto v) { assert(v == 10); });
}

void test_send_fnc() {
    int b = 3;
    Function<int(int)> f([=] (int a) { return a * b; });

    test_send(f, [] (Comm&, auto f) {
        assert(f(3) == 9);
    });
}

void test_send_taskloaf_fnc() {
    Function<int(const Comm&)> f(mpi_rank);
    test_send(f, [] (Comm& c, auto f) {
        std::cout << f(c) << std::endl;
    });
}

int main() {
    MPI_Init(nullptr, nullptr);

    test_send_simple();
    test_send_fnc();
    test_send_taskloaf_fnc();

    MPI_Finalize();
}


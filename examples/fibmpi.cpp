#include "taskloaf.hpp"
#include <mpi.h>

namespace tl = taskloaf;

int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}

tl::future<int> fib(int idx) {
    if (idx < 3) {
        return tl::ready(1);
    } else {
        auto b_fut = tl::task([=] { return fib(idx - 1); }).unwrap();

        return fib(idx - 2)
            .then([=] (tl::future<int> b_fut, int a) {
                return b_fut.then([=] (int b) { return a + b; });
            }, b_fut)
            .unwrap();
    }
}

int main(int, char** argv) {
    MPI_Init(NULL, NULL);
    {
        int n = std::stoi(std::string(argv[1]));
        tl::config cfg;
        cfg.print_stats = true;
        auto ctx = tl::launch_mpi(cfg);
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (rank == 0) {
            int x = fib(n).get();
            std::cout << "fib(" << n << ") = " << x << std::endl;
        }
    }
    MPI_Finalize();
}

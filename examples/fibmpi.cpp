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

// tl::Future<int> fib(int index) {
//     if (index < 3) {
//         return tl::ready(1);//([] () { return 1; });
//     } else {
//         return tl::when_both(fib(index - 1), fib(index - 2))
//             .then([] (int a, int b) { return a + b; });
//         // return tl::async([=] () {
//         //     return tl::when_all(
//         //         fib(index - 1),
//         //         fib(index - 2)
//         //     ).then(
//         //         [] (int a, int b) { return a + b; }
//         //     );
//         // }).unwrap();
//     }
// }

tl::future fib(int idx) {
    if (idx < 3) {
        return tl::ready(1);
    } else {
        return tl::async([idx] (tl::ignore&, tl::ignore&) {
            return fib(idx - 1).then(tl::closure(
                [] (tl::future& a, int x) {
                    return a.then([x] (tl::ignore&, int y) { return x + y; });
                },
                fib(idx - 2)
            )).unwrap();
        }).unwrap();
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

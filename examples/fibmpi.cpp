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

tl::Future<int> fib(int index) {
    if (index < 3) {
        return tl::ready(1);//([] () { return 1; });
    } else {
        return tl::when_both(fib(index - 1), fib(index - 2))
            .then([] (int a, int b) { return a + b; });
        // return tl::async([=] () {
        //     return tl::when_all(
        //         fib(index - 1),
        //         fib(index - 2)
        //     ).then(
        //         [] (int a, int b) { return a + b; }
        //     );
        // }).unwrap();
    }
}

int main(int, char** argv) {
    MPI_Init(NULL, NULL);
    {
        int n = std::stoi(std::string(argv[1]));
        auto ctx = tl::launch_mpi();
        int x = fib(n).get();
        std::cout << "fib(" << n << ") = " << x << std::endl;
    }
    MPI_Finalize();
}

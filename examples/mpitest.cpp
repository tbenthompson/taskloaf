#include <mpi.h>

#include <functional>

#include "taskloaf.hpp"
#include "taskloaf/address.hpp"
#include "taskloaf/ivar.hpp"
#include "taskloaf/worker.hpp"

using namespace taskloaf;

void mpi_launch_helper(std::function<IVarRef()> f) {
    (void)f;
    // PASS MPICOMM!
    // Worker w();
    // cur_worker = &w;
    // w.run();
    // w.set_core_affinity(i);
    // if (i == 0) {
    //     root_addr = w.get_addr();
    //     ready = true;
    //     f();
    // } else {
    //     while (!ready) {}
    //     w.introduce(root_addr); 
    // }
}

template <typename F>
void mpi_launch(F&& f) {
    MPI_Init(NULL, NULL);

    mpi_launch_helper([f = std::forward<F>(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });

    MPI_Finalize();
}

int main(int, char**) {
    mpi_launch([] () {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        std::cout << rank << std::endl;
        return ready(shutdown());
    });
}

from mpi4py import MPI
import _pytest.runner

from taskloaf.mpi import MPIComm

def pytest_runtest_protocol(item, nextitem):
    n_procs = getattr(item.function, 'n_procs', 1)
    rank = MPI.COMM_WORLD.Get_rank()
    size = MPI.COMM_WORLD.Get_size()


    if n_procs > size:
        print(
            'skipping ' + str(item.name) +
            ' because it needs ' + str(n_procs) + ' MPI procs'
        )
        return True

    if rank >= n_procs:
        MPI.COMM_WORLD.Barrier()
        return True
    else:
        c = MPIComm()
        for i in range(50):
            v = c.recv()
            if v is not None:
                print('found msg!')
        MPI.COMM_WORLD.Barrier()
        return _pytest.runner.pytest_runtest_protocol(item, nextitem)

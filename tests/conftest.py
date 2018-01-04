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
        new_comm = MPI.COMM_WORLD.Split()
        MPI.COMM_WORLD.Barrier()
        return True
    else:
        new_comm = MPI.COMM_WORLD.Split()
        MPIComm.default_comm = new_comm
        res = _pytest.runner.pytest_runtest_protocol(item, nextitem)
        MPI.COMM_WORLD.Barrier()
        return res

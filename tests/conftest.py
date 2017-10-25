from mpi4py import MPI
import _pytest.runner

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
        return True
    return _pytest.runner.pytest_runtest_protocol(item, nextitem)

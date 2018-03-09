import sys
import logging
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
        new_comm = MPI.COMM_WORLD.Split(color = 0)
        MPI.COMM_WORLD.Barrier()
        return True
    else:
        new_comm = MPI.COMM_WORLD.Split(color = 1)
        MPIComm.default_comm = new_comm
        res = _pytest.runner.pytest_runtest_protocol(item, nextitem)
        MPI.COMM_WORLD.Barrier()
        return res

def pytest_addoption(parser):
    parser.addoption('--shouldlog', action='store_true', help='display logging')

def pytest_configure(config):
    level = logging.ERROR
    if config.getoption('--shouldlog'):
        level = logging.DEBUG
    logging.basicConfig(
        format = '[%(relativeCreated)d:%(levelname)s:%(name)s] %(message)s',
        stream = sys.stdout,
        level = level,
        datefmt='%j:%H:%M:%S'
    )

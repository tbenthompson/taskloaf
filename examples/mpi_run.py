import os
from mpi4py.futures import MPIPoolExecutor

from taskloaf.mpi_comm import MPIComm
from local_run import die, run

def mpirun(n, f, tag = 0):
    this_dir = os.path.dirname(os.path.realpath(__file__))
    with MPIPoolExecutor(max_workers = n, path = [this_dir]) as p:
        return p.starmap(mpistart, zip([f] * n, range(n), [tag] * n))

def mpistart(f, i, tag):
    try:
        c = MPIComm(tag)
        f(c)
    except Exception as e:
        import traceback
        # mpi4py MPIPoolExecutor supresses stderr until the pool is done...
        # so send the exception info to stdout
        import sys
        traceback.print_exc(file = sys.stdout)
        raise e

if __name__ == "__main__":
    mpirun(2, run)

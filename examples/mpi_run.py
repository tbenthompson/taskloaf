from taskloaf.mpi_comm import MPIComm

from local_task import die, run

from mpi4py.futures import MPIPoolExecutor

def start(i):
    c = MPIComm(0)
    run(c)

import os
this_dir = os.path.dirname(os.path.realpath(__file__))
if __name__ == "__main__":
    n = 2
    p = MPIPoolExecutor(max_workers = n, path = [this_dir])
    vals = p.map(start, range(n))

    # c = MPIComm(0)
    # run(c)

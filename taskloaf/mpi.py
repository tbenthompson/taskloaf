import os
import inspect
import asyncio
from mpi4py import MPI
from mpi4py.futures import MPIPoolExecutor
import numpy as np

import taskloaf.worker
from taskloaf.serialize import dumps, loads

def rank(comm = MPI.COMM_WORLD):
    return comm.Get_rank()

def max_tag(comm = MPI.COMM_WORLD):
    return comm.Get_attr(MPI.TAG_UB)

class StartMsg:
    def __init__(self, n_items):
        self.n_items = n_items

class MPIComm:
    next_tag = 0

    # multi part messages can be done with tags.

    def __init__(self, tag = None):
        self.comm = MPI.COMM_WORLD
        self.addr = rank(self.comm)
        if tag is None:
            tag = MPIComm.next_tag
        self.tag = tag
        MPIComm.next_tag += 1

    def send(self, to_addr, data):
        self.comm.Isend(data.obj, dest = to_addr, tag = self.tag)

    def recv(self):
        s = MPI.Status()
        msg_exists = self.comm.iprobe(tag = self.tag, status = s)
        if not msg_exists:
            return None
        out = memoryview(bytearray(s.count))
        self.comm.Recv(out, source = s.source, tag = self.tag)
        return out

def mpirun(n_workers, f, tag = None):
    # D = os.path.dirname(inspect.stack()[-1].filename)
    # # this_dir = os.path.dirname(os.path.realpath(__file__))
    mpi_args = dict(max_workers = n_workers)#, path = [])
    with MPIPoolExecutor(**mpi_args) as p:
        out = p.starmap(mpistart, zip([dumps(f)] * n_workers, range(n_workers), [tag] * n_workers))
        return next(out)

def mpistart(f, i, tag):
    try:
        c = MPIComm(tag)
        return loads(None, f)(c)
    except Exception as e:
        import traceback
        # mpi4py MPIPoolExecutor supresses stderr until the pool is done...
        # so send the exception info to stdout
        import sys
        traceback.print_exc(file = sys.stdout)
        raise e

def mpiexisting(n_workers, f, tag = None):
    n_mpi_procs = MPI.COMM_WORLD.Get_size()
    if n_workers > n_mpi_procs:
        raise Exception(
            'There are only %s MPI processes but %s were requested' % (n_mpi_procs, n_workers)
        )
    c = MPIComm(tag)
    if c.addr >= n_workers:
        return
    out = f(c)
    return out

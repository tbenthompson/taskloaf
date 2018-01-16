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

import cppimport.import_hook
import taskloaf.cppworker
MPIComm = taskloaf.cppworker.MPIComm
# class MPIComm:
#     default_comm = MPI.COMM_WORLD
#
#     # multi part messages can be done with tags.
#     def __init__(self, comm = None):
#         if comm is None:
#             comm = MPIComm.default_comm
#         self.comm = comm
#         self.addr = rank(self.comm)
#
#     def send(self, to_addr, data):
#         print(self.addr, len(data))
#         self.comm.Isend(data.obj, dest = to_addr)
#
#     def recv(self):
#         s = MPI.Status()
#         msg_exists = self.comm.iprobe(status = s)
#         if not msg_exists:
#             return None
#         out = memoryview(bytearray(s.count))
#         self.comm.Recv(out, source = s.source)
#         return out

def mpirun(n_workers, f):
    # D = os.path.dirname(inspect.stack()[-1].filename)
    # # this_dir = os.path.dirname(os.path.realpath(__file__))
    mpi_args = dict(max_workers = n_workers)#, path = [])
    with MPIPoolExecutor(**mpi_args) as p:
        out = p.starmap(mpistart, zip([dumps(f)] * n_workers, range(n_workers)))
        return next(out)

def mpistart(f, i):
    c = MPIComm()
    return loads(None, f)(c)

def mpiexisting(n_workers, f):
    n_mpi_procs = MPI.COMM_WORLD.Get_size()
    if n_workers > n_mpi_procs:
        raise Exception(
            'There are only %s MPI processes but %s were requested' % (n_mpi_procs, n_workers)
        )
    c = MPIComm()
    if c.addr >= n_workers:
        return
    out = f(c)
    return out

import os
import sys
import cloudpickle
from mpi4py import MPI
from mpi4py.futures import MPIPoolExecutor

import taskloaf.worker

def rank(comm = MPI.COMM_WORLD):
    return comm.Get_rank()

# import cppimport.import_hook
# import taskloaf.cppworker
# MPIComm = taskloaf.cppworker.MPIComm
class MPIComm:
    default_comm = MPI.COMM_WORLD

    # TODO: multi part messages can be done with tags.
    def __init__(self, comm = None):
        if comm is None:
            comm = MPIComm.default_comm
        self.comm = comm
        self.addr = rank(self.comm)

    def send(self, to_addr, data):
        self.comm.Isend(data, dest = to_addr)

    def recv(self):
        s = MPI.Status()
        msg_exists = self.comm.iprobe(status = s)
        if not msg_exists:
            return None
        out = memoryview(bytearray(s.count))
        self.comm.Recv(out, source = s.source)
        return out

    def barrier(self):
        self.comm.Barrier()

def mpirun(n_workers, f, cfg):
    mpi_args = dict(max_workers = n_workers)#, path = [])
    with MPIPoolExecutor(**mpi_args) as p:
        out = p.starmap(mpistart, zip([cloudpickle.dumps(f)] * n_workers, range(n_workers)))
        return next(out)

def mpistart(f, i):
    c = MPIComm()
    return cloudpickle.loads(f)(c)

def mpiexisting(n_workers, f, cfg, die_on_exception = True):
    orig_sys_except = sys.excepthook
    if die_on_exception:
        def die(*args, **kwargs):
            orig_sys_except(*args, **kwargs)
            sys.stderr.flush()
            MPI.COMM_WORLD.Abort()
        sys.excepthook = die

    n_mpi_procs = MPI.COMM_WORLD.Get_size()
    if n_workers > n_mpi_procs:
        raise Exception(
            'There are only %s MPI processes but %s were requested' % (n_mpi_procs, n_workers)
        )
    c = MPIComm()
    out = f(c) if c.addr < n_workers else None
    sys.excepthook = orig_sys_except
    return out

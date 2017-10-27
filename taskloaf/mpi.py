import os
import inspect
import cloudpickle
from mpi4py import MPI
from mpi4py.futures import MPIPoolExecutor

MPI.pickle.__init__(cloudpickle.dumps, cloudpickle.loads)

class MPIComm:
    next_tag = 0

    def __init__(self, tag):
        self.comm = MPI.COMM_WORLD
        self.addr = self.comm.Get_rank()
        self.tag = MPIComm.next_tag
        self.tag = tag
        MPIComm.next_tag += 1

    def send(self, to_addr, data):
        # I could potentially used isend, or maybe Ibsend here to avoid the
        # blocking nature of send.
        req = self.comm.send(data, dest = to_addr, tag = self.tag)

    def recv(self):
        s = MPI.Status()
        msg_exists = self.comm.iprobe(tag = self.tag, status = s)
        if not msg_exists:
            return None
        return self.comm.recv(source = s.source, tag = self.tag)

def mpirun(n_workers, f, tag = 0):
    n_procs = n_workers + 1
    D = os.path.dirname(inspect.stack()[-1].filename)
    # this_dir = os.path.dirname(os.path.realpath(__file__))
    with MPIPoolExecutor(max_workers = n_procs, path = [D]) as p:
        return p.starmap(mpistart, zip([f] * n_procs, range(n_procs), [tag] * n_procs))

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


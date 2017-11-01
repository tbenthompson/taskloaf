import os
import inspect
import cloudpickle
import asyncio
from mpi4py import MPI
from mpi4py.futures import MPIPoolExecutor

import taskloaf.worker

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

    # NOTE: duplicated with LocalComm... not a big deal and probably the best thing to do
    async def comm_poll(self, tasks):
        while taskloaf.worker.running:
            t = self.recv()
            if t is not None:
                tasks.append(t)
            await asyncio.sleep(0)

def mpirun(n_workers, f, tag = 0):
    D = os.path.dirname(inspect.stack()[-1].filename)
    # this_dir = os.path.dirname(os.path.realpath(__file__))
    mpi_args = dict(max_workers = n_workers, path = [D])
    with MPIPoolExecutor(**mpi_args) as p:
        out = p.starmap(mpistart, zip([f] * n_workers, range(n_workers), [tag] * n_workers))
        return next(out)

def mpistart(f, i, tag):
    try:
        c = MPIComm(tag)
        return f(c)
    except Exception as e:
        import traceback
        # mpi4py MPIPoolExecutor supresses stderr until the pool is done...
        # so send the exception info to stdout
        import sys
        traceback.print_exc(file = sys.stdout)
        raise e

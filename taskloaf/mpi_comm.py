import cloudpickle
from mpi4py import MPI
MPI.pickle.dumps = cloudpickle.dumps
MPI.pickle.loads = cloudpickle.loads

class MPIComm:
    next_tag = 0

    def __init__(self, tag):
        self.comm = MPI.COMM_WORLD
        self.addr = self.comm.Get_rank()
        self.tag = MPIComm.next_tag
        self.tag = tag
        MPIComm.next_tag += 1

    def send(self, to_addr, data):
        req = self.comm.isend(data, dest = to_addr, tag = self.tag)

    def recv(self):
        s = MPI.Status()
        msg_exists = self.comm.iprobe(tag = self.tag, status = s)
        if not msg_exists:
            return None
        return self.comm.recv(source = s.source, tag = self.tag)

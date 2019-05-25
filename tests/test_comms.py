import cloudpickle
from multiprocessing import Process, Queue
from mpi4py import MPI
from taskloaf.local import LocalComm
from taskloaf.mpi import MPIComm
from taskloaf.zmq import ZMQComm, zmqrun
from taskloaf.test_decorators import mpi_procs

test_cfg = dict()

def queue_test_helper(q):
    q.put(123)

def test_queue():
    q = Queue()
    p = Process(target = queue_test_helper, args = (q, ))
    p.start()
    assert(q.get() == 123)
    p.join()

def test_local_comm():
    qs = [Queue(), Queue()]
    c0 = LocalComm(qs, 0)
    c1 = LocalComm(qs, 1)
    c0.send(1, cloudpickle.dumps(456))
    go = True
    while go:
        val = c1.recv()
        if val is not None:
            assert(cloudpickle.loads(val) == 456)
            go = False

def test_empty_recv():
    c = MPIComm()
    assert(c.recv() is None)

@mpi_procs(2)
def test_zmq_data_send():
    data_send(MPIComm())

@mpi_procs(2)
def test_zmq_fnc_send():
    fnc_send(MPIComm())

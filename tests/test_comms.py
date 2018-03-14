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

def test_tcp():
    pass

def wait_for(c, val, get = lambda x: x):
    stop = False
    while not stop:
        msg = c.recv()
        if msg is not None:
            assert(cloudpickle.loads(msg) == get(val))
        stop = True

def test_zmq():
    n_workers = 4
    leader = 1
    def f(c):
        if c.addr == leader:
            for i in range(n_workers):
                if i == c.addr:
                    continue
                c.send(i, cloudpickle.dumps("OHI!"))
            for i in range(n_workers - 1):
                wait_for(c, "OYAY")
        else:
            wait_for(c, "OHI!")
            c.send(leader, cloudpickle.dumps("OYAY"))
        c.barrier()
    zmqrun(n_workers, f, test_cfg)

def data_send(c):
    if c.addr == 0:
        c.send(1, cloudpickle.dumps(123))
    if c.addr == 1:
        wait_for(c, 123)

def fnc_send(c):
    if c.addr == 0:
        x = 13
        c.send(1, cloudpickle.dumps(lambda: x ** 2))
    if c.addr == 1:
        wait_for(c, 169, lambda x: x())

def test_zmq_data_send():
    zmqrun(2, data_send, test_cfg)

def test_zmq_fnc_send():
    zmqrun(2, fnc_send, test_cfg)

@mpi_procs(2)
def test_zmq_data_send():
    data_send(MPIComm())

@mpi_procs(2)
def test_zmq_fnc_send():
    fnc_send(MPIComm())

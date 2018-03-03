from multiprocessing import Process, Queue
from mpi4py import MPI
from taskloaf.local import LocalComm
from taskloaf.mpi import MPIComm
from taskloaf.test_decorators import mpi_procs
from taskloaf.serialize import dumps, loads

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
    c0.send(1, memoryview(dumps(456)[1]))
    go = True
    while go:
        val = c1.recv()
        if val is not None:
            assert(loads(None, [], val) == 456)
            go = False

def test_empty_recv():
    c = MPIComm()
    assert(c.recv() is None)

@mpi_procs(2)
def test_data_send():
    c = MPIComm()
    if c.addr == 0:
        c.send(1, memoryview(dumps(123)[1]))
    if c.addr == 1:
        go = True
        while go:
            data = c.recv()
            if data is not None:
                go = False
                assert(loads(None, [], data) == 123)

@mpi_procs(2)
def test_fnc_send():
    c = MPIComm()
    if c.addr == 0:
        x = 13
        c.send(1, memoryview(dumps(lambda: x ** 2)[1]))
    if c.addr == 1:
        go = True
        while go:
            data = c.recv()
            if data is not None:
                f = loads(None, [], data)
                go = False
                assert(f() == 169)

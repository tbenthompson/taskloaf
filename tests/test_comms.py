from multiprocessing import Process, Queue
from mpi4py import MPI
from taskloaf.local_comm import LocalComm
from taskloaf.mpi_comm import MPIComm

def mpi_procs(n):
    n_procs_available = MPI.COMM_WORLD.Get_size()
    def decorator(test_fnc):
        test_fnc.n_procs = n
        return test_fnc
    return decorator

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
    c0.send(1, 456)
    assert(c1.recv() == 456)

def test_tag_increment():
    c1 = MPIComm(0)
    c2 = MPIComm(1)
    assert(c2.tag == c1.tag + 1)

def test_empty_recv():
    c = MPIComm(2)
    assert(c.recv() is None)

@mpi_procs(2)
def test_data_send():
    c = MPIComm(3)
    if c.addr == 0:
        c.send(1, 123)
    if c.addr == 1:
        go = True
        while go:
            data = c.recv()
            if data is not None:
                go = False
                assert(data == 123)

@mpi_procs(2)
def test_fnc_send():
    c = MPIComm(4)
    if c.addr == 0:
        x = 13
        c.send(1, lambda: x ** 2)
    if c.addr == 1:
        go = True
        while go:
            data = c.recv()
            if data is not None:
                go = False
                assert(data() == 169)

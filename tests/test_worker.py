import os
import pytest
import asyncio
import taskloaf.worker
from taskloaf.cluster import cluster
from taskloaf.mpi import mpiexisting, MPIComm, rank
from taskloaf.test_decorators import mpi_procs
from taskloaf.run import run
from fixtures import w

if __name__ == "__main__":
    test_log()

def test_shutdown(w):
    async def f(w):
        taskloaf.worker.shutdown(w)
    w.start(f)

def test_run_work():
    val = [0, 1]
    y = 2.0
    async def f(w):
        def g(w, x):
            val[0] = x
        w.run_work(g, y)
    run(f)
    assert(val[0] == y)

def test_await_work():
    val = [0]
    async def f(w):
        def h(w, x):
            val[0] = x
        S = 'dang'
        await w.wait_for_work(h, S)
        assert(val[0] == S)
    run(f)

def test_run_output():
    async def f(w):
        return 1
    assert(run(f) == 1)

@mpi_procs(2)
def test_remote_work():
    async def f(w):
        if w.addr != 0:
            return
        async def g(w):
            assert(w.addr == 1)
            def h(w):
                assert(w.addr == 0)
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
            taskloaf.worker.shutdown(w)
        w.submit_work(1, g)
        while True:
            await asyncio.sleep(0)
    cluster(2, f, runner = mpiexisting)

def test_cluster_output():
    async def f(w):
        return 1
    assert(cluster(1, f) == 1)

class FunnyException(Exception):
    pass

@mpi_procs(2)
def test_cluster_death_cleansup():
    def check(n, onoff):
        for i in range(n):
            path = '/dev/shm/taskloaf_' + str(i) + '_0'
            assert(os.path.exists(path) == onoff)
    async def f(w):
        ptr = w.allocator.malloc(1)
        check(1, True)
        raise FunnyException()
    check(2, False)
    raises = False
    try:
        cluster(2, f, runner = mpiexisting)
    except FunnyException as e:
        raises = True
    assert(rank() == 1 or raises)
    import gc; gc.collect()
    check(2, False)

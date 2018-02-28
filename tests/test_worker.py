import os
import pytest
import taskloaf.worker
from taskloaf.run import run

def test_shutdown():
    async def f(w):
        taskloaf.worker.shutdown(w)
    with taskloaf.worker.Worker(taskloaf.worker.NullComm()) as w:
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

from taskloaf.cluster import cluster
from taskloaf.mpi import mpiexisting, MPIComm, rank
from taskloaf.test_decorators import mpi_procs
import asyncio
@mpi_procs(2)
def test_remote_work():
    async def f(w):
        if w.addr != 0:
            return
        def g(w):
            assert(w.addr == 1)
            def h(w):
                assert(w.addr == 0)
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
            taskloaf.worker.shutdown(w)
        w.submit_work(1, g)
        while True:
            await asyncio.sleep(0)
    taskloaf.worker.Worker(MPIComm()).start(f)

def test_cluster_output():
    async def f(w):
        return 1
    assert(cluster(1, f) == 1)

@mpi_procs(2)
def test_cluster_death_cleansup():
    def check(n, onoff):
        for i in range(n):
            path = '/dev/shm/taskloaf_' + str(i) + '_0'
            assert(os.path.exists(path) == onoff)
    async def f(w):
        ptr = w.allocator.malloc(1)
        check(1, True)
        raise Exception("HI")
    check(2, False)
    try:
        cluster(2, f)
    except Exception as e:
        print(e)
    check(2, False)

# When originally written, this test hung forever. So, even though there are no
# asserts, it is testing *something*. It checks that the exception behavior
# propagates from free tasks upward to the Worker properly.
# @mpi_procs(2)
# def test_cluster_broken_task():
#     async def f(w):
#
#         async def f(w):
#             while True:
#                 await asyncio.sleep(0)
#
#         async def broken_task(w):
#             print(x)
#
#         taskloaf.task(w, f)
#         await taskloaf.task(w, broken_task, to = 1)
#     if rank() == 1:
#         with pytest.raises(NameError):
#             cluster(2, f)
#     else:
#         cluster(2, f)

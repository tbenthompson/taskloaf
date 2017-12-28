import taskloaf.worker
from taskloaf.run import run

def test_shutdown():
    async def f(w):
        taskloaf.worker.shutdown(w)
    taskloaf.worker.Worker(taskloaf.worker.NullComm()).start(f)

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
from taskloaf.mpi import mpiexisting
from taskloaf.test_decorators import mpi_procs
import asyncio
@mpi_procs(2)
def test_remote_work():
    async def f(w):
        def g(w):
            assert(w.addr == 1)
            def h(w):
                assert(w.addr == 0)
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
        w.submit_work(1, g)
        while w.running:
            await asyncio.sleep(0)
    cluster(2, f, runner = mpiexisting)

def test_cluster_output():
    async def f(w):
        return 1
    assert(cluster(1, f, runner = mpiexisting) == 1)

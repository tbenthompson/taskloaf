from taskloaf.worker import local_task, task_loop, shutdown, start_worker
from taskloaf.test_decorators import mpi_procs
from taskloaf.mpi import MPIComm
import asyncio

def test_shutdown():
    async def f():
        shutdown()
    start_worker(None, f())

def test_closure():
    def f():
        f.x = f.y
        shutdown()
    f.x = 1
    f.y = 2
    async def g():
        local_task(f)
    start_worker(None, g())
    assert(f.x == 2)

def test_coroutine():
    x = [0]

    def f():
        x[0] += 1

    async def g():
        x[0] += 1
        await asyncio.sleep(0)
        x[0] *= 2
        shutdown()

    async def h():
        local_task(f)
        local_task(g)

    start_worker(None, h())
    assert(x[0] == 4)

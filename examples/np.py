import time
import numpy as np
import taskloaf as tsk

async def run(w, r):
    async def remote(w):
        return np.sum(await tsk.remote_get(r))
    assert(await tsk.task(w, remote, to = 1) == np.sum(r.get()))

async def submit(w):
    n = int(3e7)
    r = w.memory.put(np.random.rand(n))
    await run(w, r)
    r2 = w.memory.put(np.random.rand(n))
    async with tsk.Profiler(w, range(2)):
        await run(w, r2)

if __name__ == "__main__":
    tsk.cluster(2, submit, runner = tsk.mpiexisting)

import mmap
import time
import pickle
import numpy as np
import taskloaf as tsk

async def run(w, r):
    async def remote(w):
        d = pickle.loads(await tsk.remote_get(r))
        return np.sum(d)
    lhs = pickle.loads(await tsk.task(w, remote, to = 1))
    rhs = np.sum(r.get())
    assert(lhs == rhs)

n = int(3e7)
async def submit(w):
    A = np.random.rand(n)
    r = w.memory.put(A)
    await run(w, r)
    # r2 = w.memory.put(np.random.rand(n))
    # async with tsk.Profiler(w, range(2)):
    #     await run(w, r2)

if __name__ == "__main__":
    tsk.cluster(2, submit, runner = tsk.mpiexisting)

import mmap
import time
import pickle
import numpy as np
import taskloaf as tsk
from taskloaf.shmem import alloc_shmem, Shmem

def sum_shm(sm):
    return np.sum(np.frombuffer(sm.mem))

async def run_shm(w, sm):
    async def remote(w):
        return sum_shm(Shmem(sm.filename))
    rhs = sum_shm(sm)
    lhs = pickle.loads(await tsk.task(w, remote, to = 1))
    assert(lhs == rhs)

n = int(1e8)
async def submit_shm(w):
    A = np.random.rand(n)
    async with tsk.Profiler(w, range(2)):
        with alloc_shmem('block', A) as sm:
            await run_shm(w, sm)

async def run_pkl(w, r):
    async def remote(w):
        d = pickle.loads(await tsk.remote_get(r))
        return np.sum(d)
    lhs = pickle.loads(await tsk.task(w, remote, to = 1))
    rhs = np.sum(r.get())
    assert(lhs == rhs)

n = int(3e7)
async def submit_pkl(w):
    A = np.random.rand(n)
    r = w.memory.put(A)
    await run_pkl(w, r)
    r2 = w.memory.put(np.random.rand(n))
    async with tsk.Profiler(w, range(2)):
        await run_pkl(w, r2)

if __name__ == "__main__":
    tsk.cluster(2, submit_pkl, runner = tsk.mpiexisting)
    tsk.cluster(2, submit_shm, runner = tsk.mpiexisting)

import numpy as np
import taskloaf as tsk
import taskloaf.profile
import taskloaf.serialize
import time

# uvloop is faster!

import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

n_jobs = 10000
job_size = 1000
def invert(w, A):
    Ainv = np.linalg.inv(A)
    return None

def long_fnc(i):
    x = 4
    for i in range(job_size):
        if i % 2 == 0:
            x -= i
        else:
            x *= 2
    # A = np.random.rand(n,n)
    # Ainv = np.linalg.inv(A)
    return x.to_bytes(8, 'little')

def caller(i):
    return long_fnc()

def run_once():
    for i in range(n_jobs):
        long_fnc(i)

def run_mp_parallel():
    import multiprocessing
    p = multiprocessing.Pool(2)
    p.map(caller, range(n_jobs))
    p.close()
    p.join()

async def wait_all(jobs):
    results = []
    for j in jobs:
        results.append(await j)
    return results

def run_tsk_parallel():
    n_cores = 2
    async def submit(w):
        # A = np.random.rand(job_size, job_size)
        # r = w.memory.put(A)
        # def fnc(w):
        #     A_inside = tsk.remote_get(r)
        #     return invert(w, A_inside)
        fnc_dref = w.memory.put(
            serialized = taskloaf.serialize.dumps(lambda w: long_fnc(0))
        )
        async with tsk.profile.Profiler(w, range(n_cores)):
            start = time.time()
            out = await wait_all([
                tsk.task(w, fnc_dref, to = i % n_cores) for i in range(n_jobs)
            ])
            print('inside: ', time.time() - start)

    return tsk.cluster(n_cores, submit)

def run_dask_parallel():
    import dask
    return dask.compute(*[dask.delayed(long_fnc)() for i in range(n_jobs)])

start = time.time()
end_times = run_tsk_parallel()
print('taskloaf took: ', time.time() - start)
#
# if tsk.mpi.rank() == 0:
#     start = time.time()
#     run_dask_parallel()
#     print('dask took: ', time.time() - start)
#
if tsk.mpi.rank() == 0:
    start = time.time()
    run_once()
    print('singlethreaded took: ', time.time() - start)
#
# if tsk.mpi.rank() == 0:
#     start = time.time()
#     run_mp_parallel()
#     print('multiprocessing took: ', time.time() - start)

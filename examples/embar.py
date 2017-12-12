import numpy as np
import taskloaf as tsk
import time

n_jobs = 10
def long_fnc():
    job_size = 1000000
    x = 4
    for i in range(job_size):
        if i % 2 == 0:
            x -= i
        else:
            x *= 2
    # A = np.random.rand(n,n)
    # Ainv = np.linalg.inv(A)
    return x

def caller(i):
    return long_fnc()

def run_once():
    for i in range(n_jobs):
        long_fnc()

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
    async def submit():
        # async def groupjob():
        #     return await wait_all([tsk.task(long_fnc) for i in range(n_jobs)])
        # return await wait_all([tsk.task(groupjob, to = i) for i in range(2)])
        return await wait_all([tsk.task(long_fnc, to = i % 2) for i in range(n_jobs)])
    return tsk.cluster(2, submit)#, runner = tsk.mpi.mpiexisting)

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

import numpy as np
import taskloaf as tsk
import time

n_jobs = 1000
job_size = 10000
def long_fnc():
    x = 4
    for i in range(job_size):
        if i % 2 == 0:
            x -= i
        else:
            x *= 2
    # A = np.random.rand(n,n)
    # Ainv = np.linalg.inv(A)
    return time.time()

def caller(i):
    return long_fnc()

def run_once():
    for i in range(n_jobs):
        long_fnc()

def run_two_parallel():
    import multiprocessing
    p = multiprocessing.Pool(2)
    p.map(caller, range(8))
    p.close()
    p.join()

async def wait_all(jobs):
    results = []
    for j in jobs:
        results.append(await j)
    return results

def run_tsk_parallel():
    async def submit():
        async def groupjob():
            return await wait_all([tsk.task(long_fnc) for i in range(n_jobs)])
        return await wait_all([tsk.task(groupjob, to = i) for i in range(2)])
        # return await wait_all([tsk.task(long_fnc, to = i % 2) for i in range(n_jobs * 2)])

    return tsk.cluster(2, submit, runner = tsk.mpi.mpiexisting)

start = time.time()
end_times = run_tsk_parallel()
print('took: ', time.time() - start)
# tsk.profile_stats()

start = time.time()
if tsk.mpi.rank() == 0:
    run_once()
# run_two_parallel()
print('took: ', time.time() - start)

# tsk.profile_stats()

import numpy as np
import taskloaf as tsk
import time

n_jobs = 100
def long_fnc():
    job_size = 100000
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
    n_cores = 2
    async def submit(w):
        from pyinstrument import Profiler

        def start_profiler(w):
            profiler = Profiler()
            profiler.start()
            return profiler

        def stop_profiler(profiler, i):
            profiler.stop()
            print('profile for core: ' + str(i))
            print(profiler.output_text(unicode=True, color=True))

        prof_tsks = [tsk.task(w, start_profiler, to = i) for i in range(1, n_cores)]
        for pt in prof_tsks:
            await pt
        profiler = start_profiler(w)

        start = time.time()
        out = await wait_all([
            tsk.task(w, lambda w: long_fnc(), to = i % n_cores) for i in range(n_jobs)
        ])
        print('inside: ', time.time() - start)

        stop_profiler(profiler, 0)
        for _i, pt in enumerate(prof_tsks):
            i = _i + 1
            await pt.then(lambda w, x, i = i: stop_profiler(x, i), to = i)

    return tsk.cluster(n_cores, submit)#, runner = tsk.mpi.mpiexisting)

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

import asyncio

import uvloop

# import numpy as np

import taskloaf as tsk
import taskloaf.profile
import taskloaf.serialize
import time

# uvloop is faster!
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

n_jobs = 10
job_size = 100
n_repeats = 3000


def long_fnc():
    x = 4
    for i in range(job_size):
        if i % 2 == 0:
            x -= i
        else:
            x *= 2
    return x.to_bytes(8, "little")


def caller(i):
    return long_fnc()


def run_serial():
    start = time.time()
    for i in range(n_jobs):
        long_fnc()
    print("serial", time.time() - start)


def run_mp_parallel():
    import multiprocessing

    start = time.time()
    p = multiprocessing.Pool(2)
    for i in range(n_repeats):
        p.map(caller, list(range(n_jobs)))
    p.close()
    p.join()
    print("mp parallel", time.time() - start)


async def wait_all(jobs):
    results = []
    for j in jobs:
        results.append(await j)
    return results


def run_tsk_parallel():
    async def f():
        gang = await tsk.ctx().wait_for_workers(2)
        fnc_dref = tsk.object_ref.put_raw(long_fnc)
        # fnc_dref = tsk.put(long_fnc)
        async with tsk.Profiler(gang):
            start = time.time()
            # chunk_bounds = np.linspace(0, n_jobs, len(gang) + 1).astype(np.int)
            # jobs = []
            # for i in range(n_repeats):
            #     for worker_idx in range(len(gang)):
            #         n = chunk_bounds[worker_idx + 1] - chunk_bounds[worker_idx]
            #         async def run_jobs():
            #             for i in range(n):
            #                 long_fnc()
            #         jobs.append(tsk.task(run_jobs, to=gang[worker_idx]))
            #     await wait_all(jobs)
            await wait_all(
                [
                    tsk.task(fnc_dref, to=gang[i % len(gang)])
                    for i in range(n_jobs * n_repeats)
                ]
            )
            print("inside: ", time.time() - start)

    def setup_worker(name):
        tsk.cfg.stdout_logging(name, "taskloaf.profile")

    cfg = tsk.Cfg(initializer=setup_worker)
    return tsk.zmq_run(cfg=cfg, f=f)


if __name__ == "__main__":
    run_serial()
    run_mp_parallel()
    run_tsk_parallel()

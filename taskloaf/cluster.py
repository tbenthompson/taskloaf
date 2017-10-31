from taskloaf.worker import start_worker, submit_task, shutdown
from taskloaf.local import localrun

def cluster(n_workers, coro, runner = localrun):
    def wrap_start_coro(c):

        async def coro_wrapper():
            result = await coro()
            for i in range(n_workers):
                submit_task(i, shutdown)
            return result
        start_coro = coro_wrapper() if c.addr == 0 else None

        return start_worker(c, start_coro)

    return runner(n_workers, wrap_start_coro)

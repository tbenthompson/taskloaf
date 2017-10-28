from taskloaf.worker import start_worker
from taskloaf.local import localrun

def cluster(n_workers, coro, runner = localrun):
    def wrap_start_coro(c):
        start_coro = coro() if c.addr == 0 else None
        return start_worker(c, start_coro)
    return runner(n_workers, wrap_start_coro)

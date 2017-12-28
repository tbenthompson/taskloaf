import taskloaf.worker
import taskloaf.memory
import taskloaf.promise
from taskloaf.local import localrun
from taskloaf.run import default_worker

def killall(w, n_workers):
    for i in range(n_workers):
        w.submit_work(i, taskloaf.worker.shutdown)

def cluster(n_workers, coro, runner = localrun):
    def wrap_start_coro(c):
        async def setup(w):
            if w.comm.addr == 0:
                result = await coro(w)
                killall(w, n_workers)
                return result
        try:
            result = default_worker(c).start(setup)
            return result
        except Exception as e:
            killall(w, n_workers)
            raise e

    return runner(n_workers, wrap_start_coro)

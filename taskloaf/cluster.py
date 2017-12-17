import taskloaf.worker
import taskloaf.memory
import taskloaf.promise
from taskloaf.local import localrun


def cluster(n_workers, coro, runner = localrun):
    def wrap_start_coro(c):
        async def setup(w):
            if w.comm.addr == 0:
                result = await coro(w)
                for i in range(n_workers):
                    w.submit_work(i, taskloaf.worker.shutdown)
                return result
        w = taskloaf.worker.Worker()
        w.memory = taskloaf.memory.MemoryManager(w)
        w.promise_manager = taskloaf.promise.PromiseManager(w)
        return w.start(c, [setup])[0]

    return runner(n_workers, wrap_start_coro)

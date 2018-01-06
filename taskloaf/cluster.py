import taskloaf.worker
from taskloaf.mpi import mpiexisting

def killall(w, n_workers):
    for i in range(n_workers):
        w.submit_work(i, taskloaf.worker.shutdown)

def cluster(n_workers, coro, runner = mpiexisting):
    def wrap_start_coro(c):
        async def setup(worker):
            if worker.comm.addr == 0:
                out = await coro(worker)
                killall(worker, n_workers)
                return out
        with taskloaf.worker.Worker(c) as worker:
            store = taskloaf.memory.SerializedMemoryStore(
                worker.addr, worker.exit_stack
            )
            worker.memory = taskloaf.memory.MemoryManager(worker)
            taskloaf.promise.setup_protocol(worker)
            try:
                result = worker.start(setup)
                killall(worker, n_workers)
                return result
            except Exception as e:
                killall(worker, n_workers)
                raise e

    return runner(n_workers, wrap_start_coro)

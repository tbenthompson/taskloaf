import taskloaf.worker
from taskloaf.mpi import mpiexisting
from taskloaf.run import add_plugins

def killall(w, n_workers):
    for i in range(n_workers):
        w.submit_work(i, taskloaf.worker.shutdown)

def cluster(n_workers, coro, runner = mpiexisting):
    def wrap_start_coro(c):
        async def setup(worker):
            if worker.comm.addr == 0:
                result = await coro(worker)
                killall(worker, n_workers)
                return result
        worker = add_plugins(taskloaf.worker.Worker(c))
        try:
            result = worker.start(setup)
            return result
        except Exception as e:
            killall(worker, n_workers)
            raise e

    return runner(n_workers, wrap_start_coro)

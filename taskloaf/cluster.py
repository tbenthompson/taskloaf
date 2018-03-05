import taskloaf.worker
from taskloaf.mpi import mpiexisting
from taskloaf.run import add_plugins

def killall(worker, n_workers):
    for i in range(n_workers):
        worker.submit_work(i, taskloaf.worker.shutdown)

def cluster(n_workers, coro, runner = mpiexisting):
    def wrap_start_coro(c):
        async def setup(worker):
            if worker.comm.addr == 0:
                try:
                    worker.result = await coro(worker)
                finally:
                    killall(worker, n_workers)

        with taskloaf.worker.Worker(c) as worker:
            add_plugins(worker)
            try:
                worker.result = None
                worker.start(setup)
                return worker.result
            except:
                killall(worker, n_workers)
                raise
            finally:
                c.barrier()

    return runner(n_workers, wrap_start_coro)

import taskloaf.worker
from taskloaf.mpi import mpiexisting
from taskloaf.run import add_plugins

def cluster(n_workers, coro, runner = mpiexisting):
    def wrap_start_coro(c):
        async def setup(worker):
            if worker.comm.addr == 0:
                try:
                    worker.result = await coro(worker)
                finally:
                    worker.shutdown_all(range(n_workers))

        with taskloaf.worker.Worker(c) as worker:
            add_plugins(worker)
            try:
                worker.result = None
                worker.start(setup)
                return worker.result
            except:
                worker.shutdown_all(range(n_workers))
                raise
            finally:
                c.barrier()

    return runner(n_workers, wrap_start_coro)

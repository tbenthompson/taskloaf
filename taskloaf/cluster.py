import taskloaf.worker
from taskloaf.zmq import zmqrun
from taskloaf.run import add_plugins

def cluster(n_workers, coro, cfg = None, runner = zmqrun):
    if cfg is None:
        cfg = dict()

    def wrap_start_coro(comm):
        async def setup(worker):
            if worker.comm.addr == 0:
                try:
                    worker.result = await coro(worker)
                finally:
                    worker.shutdown_all(range(n_workers))

        with taskloaf.worker.Worker(comm, cfg) as worker:
            add_plugins(worker)
            try:
                worker.result = None
                worker.start(setup)
                return worker.result
            except:
                worker.shutdown_all(range(n_workers))
                raise
            finally:
                # TODO: Is this necessary?
                comm.barrier()

    return runner(n_workers, wrap_start_coro, cfg)

import taskloaf.worker
from taskloaf.mpi import mpiexisting
from taskloaf.zmq import zmqrun
from taskloaf.run import add_plugins
from taskloaf.default_cfg import setup_cfg

#TODO: Read about best practice on python configuration
#TODO: This function is doing several things. Split it out into diff parts.
# 1) setting default cfg. --> cfg should carry the plugin list
# (functions to load the plugins). Should get called by the cluster builder.
# 2) creating and starting workers. Should be done by the cluster builder
# 3) Differentiating the behavior of the main worker and the other workers. Don't do this.
# 4) returning results from a "cluster" run. Don't do this. Replace with Client.

# 5) Unrelatedly, should I go so far as to allow the Comm to be a plugin? Could allow
# multiple comms per worker?
def cluster(n_workers, coro, cfg = None, runner = zmqrun):
    cfg = setup_cfg(cfg)

    def wrap_start_coro(comm):
        async def setup(worker):
            if worker.comm.addr == 0:
                try:
                    worker.result = await coro(worker)
                finally:
                    worker.shutdown_all(range(n_workers))

        with taskloaf.worker.Worker(comm, cfg) as worker:
            add_plugins(worker)
            worker.result = None
            worker.start(setup)
            return worker.result

    return runner(n_workers, wrap_start_coro, cfg)

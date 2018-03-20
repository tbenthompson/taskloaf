import psutil

from taskloaf.zmq import zmqstart, ZMQComm
from taskloaf.worker import Worker
from taskloaf.default_cfg import setup_cfg

# def launch_worker(cfg = None):
#     setup_cfg(cfg)
#     psutil.Process().cpu_affinity([cfg['cpu_affinity']])
#     c = ZMQComm(i, hosts)

class ZMQCluster:
    def __init__(self, n_workers, cfg = None):
        setup_cfg(cfg)

        ports = cfg.get(
            'zmq_ports',
            [5755 + 2 * i for i in range(n_workers)]
        )
        hostnames = cfg.get(
            'zmq_hostnames',
            ['tcp://127.0.0.1:%s' for i in range(n_workers)]
        )
        hosts = [(h, p) for h, p in zip(hostnames, ports)]

        comm = ZMQComm(n_workers, hosts)
        worker = Worker(comm, cfg)
        add_plugins(worker)
        async def nullf():
            pass
        worker.start(nullf)

        ps = [
            multiprocessing.Process(
                target = zmqstart,
                args = (cloudpickle.dumps(f), i, hosts)
            ) for i in range(n_workers)
        ]
        for p in ps:
            p.start()
        # finally:
        #     #TODO: What if there was an error in the previous block? Does this
        #     # finally block hang?
        for p in ps:
            p.join()

def zmqstart(f, i, hosts, q):
    n_cpus = psutil.cpu_count()
    psutil.Process().cpu_affinity([i % n_cpus])
    c = ZMQComm(i, hosts)
    out = cloudpickle.loads(f)(c)
    c.close()
    if i == 0:
        q.put(out)

# def cluster(n_workers, coro, cfg = None):
#     if cfg is None:
#         cfg = dict()
#
#     def wrap_start_coro(comm):
#         async def setup(worker):
#             if worker.comm.addr == 0:
#                 try:
#                     worker.result = await coro(worker)
#                 finally:
#                     worker.shutdown_all(range(n_workers))
#
#         with taskloaf.worker.Worker(comm, cfg) as worker:
#             add_plugins(worker)
#             worker.result = None
#             worker.start(setup)
#             return worker.result
#
#     return runner(n_workers, wrap_start_coro, cfg)

def test_client():
    ZMQCluster()

if __name__ == "__main__":
    test_client()

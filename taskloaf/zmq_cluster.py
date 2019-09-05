from contextlib import ExitStack, contextmanager
import multiprocessing

import psutil

import taskloaf
from taskloaf.zmq_comm import ZMQComm
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.context import Context
from taskloaf.executor import Executor


def zmq_launcher(addr, cpu_affinity, meet_addr):
    psutil.Process().cpu_affinity(cpu_affinity)
    with ZMQComm(addr) as comm:
        cfg = dict()
        # TODO: addr[1] = port, this is insufficient eventually, use uuid?
        # TODO: move ZMQClient/ZMQWorker to their own file
        messenger = JoinMeetMessenger(addr[1], comm, True)
        messenger.protocol.add_msg_type("COMPLETE", handler=lambda args: None)
        if meet_addr is not None:
            messenger.meet(meet_addr)
        with Context(messenger, cfg) as ctx:
            taskloaf.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            ctx.executor = Executor(ctx.messenger.recv, cfg, ctx.log)
            ctx.executor.start()


class ZMQWorker:
    def __init__(self, addr, cpu_affinity, meet_addr=None):
        self.addr = addr
        self.p = multiprocessing.Process(
            target=zmq_launcher, args=(addr, cpu_affinity, meet_addr)
        )

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # TODO: softer exit?
        self.p.terminate()
        self.p.join()


class ZMQCluster:
    def __init__(self, workers):
        self.workers = workers

    def addr(self):
        return self.workers[0].addr


@contextmanager
def zmq_cluster(n_workers=None, hostname="tcp://127.0.0.1", ports=None):
    n_cores = psutil.cpu_count(logical=False)
    if n_workers is None:
        n_workers = n_cores
    if ports is None:
        base_port = taskloaf.default_base_port
        ports = range(base_port + 1, base_port + n_workers + 1)
    with ExitStack() as es:
        workers = []
        for i in range(n_workers):
            port = ports[i]
            meet_addr = None
            if i > 0:
                meet_addr = (hostname, ports[0])
            workers.append(
                es.enter_context(
                    ZMQWorker(
                        (hostname, port), [i % n_cores], meet_addr=meet_addr
                    )
                )
            )
        yield ZMQCluster(workers)

import random
from contextlib import ExitStack, contextmanager
import multiprocessing

import psutil

import taskloaf as tsk
import taskloaf.defaults as defaults
from taskloaf.zmq_comm import ZMQComm
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.context import Context
from taskloaf.executor import Executor

import logging

logger = logging.getLogger(__name__)


def random_name():
    return random.getrandbits(64)


def setup_logging(name):
    level = logging.INFO
    tsk_log = logging.getLogger("taskloaf")
    tsk_log.setLevel(level)
    ch = logging.StreamHandler()
    ch.setLevel(level)
    formatter = logging.Formatter(
        f"{name} %(asctime)s [%(levelname)s] %(name)s: %(message)s"
    )
    ch.setFormatter(formatter)
    tsk_log.addHandler(ch)


def zmq_launcher(addr, cpu_affinity, meet_addr, f=None):
    name = random_name()
    setup_logging(name)

    logger.info(
        f"Setting up ZeroMQ-based worker with name={name}"
        f", addr={addr}, and cpu_affinity={cpu_affinity}"
    )

    if cpu_affinity is not None:
        psutil.Process().cpu_affinity(cpu_affinity)

    with ZMQComm(addr) as comm:
        cfg = dict()
        messenger = JoinMeetMessenger(name, comm, True)
        messenger.protocol.add_msg_type("COMPLETE", handler=lambda args: None)
        if meet_addr is not None:
            logger.info(f"Meeting cluster at {meet_addr}")
            messenger.meet(meet_addr)
        with Context(messenger, cfg) as ctx:
            tsk.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            logger.info("launching executor")
            ctx.executor = Executor(ctx.messenger.recv, cfg)
            if f is not None:
                logger.info(f"with task {f}")
            ctx.executor.start(f)


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
def zmq_cluster(n_workers=None, hostname=None, ports=None, connect_to=None):
    n_cores = psutil.cpu_count(logical=False)

    if hostname is None:
        hostname = defaults.localhost

    if n_workers is None:
        n_workers = n_cores

    if ports is None:
        base_port = defaults.base_port
        ports = range(base_port + 1, base_port + n_workers + 1)

    if connect_to is None:
        connect_to = (hostname, ports[0])
    with ExitStack() as es:
        workers = []
        for i in range(n_workers):
            port = ports[i]
            addr = (hostname, port)
            meet_addr = connect_to
            if meet_addr == addr:
                meet_addr = None
            workers.append(
                es.enter_context(
                    ZMQWorker(addr, [i % n_cores], meet_addr=meet_addr)
                )
            )
        yield ZMQCluster(workers)

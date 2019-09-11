import random
from contextlib import ExitStack
import multiprocessing

import psutil

import taskloaf as tsk
from taskloaf.cfg import Cfg
from taskloaf.zmq_comm import ZMQComm
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.context import Context
from taskloaf.executor import Executor

import logging

logger = logging.getLogger(__name__)


def random_name():
    return random.getrandbits(64)


def zmq_launcher(cfg):
    name = random_name()

    if cfg.initializer is not None:
        cfg.initializer(name)

    logger.info(
        f"Setting up ZeroMQ-based worker with name={name}"
        f", addr={cfg.addr()}, and cpu_affinity={cfg.affinity[0]}"
    )

    if cfg.affinity is not None:
        psutil.Process().cpu_affinity(cfg.affinity[0])

    with ZMQComm(cfg.addr()) as comm:
        messenger = JoinMeetMessenger(name, comm, True)
        messenger.protocol.add_msg_type("COMPLETE", handler=lambda args: None)
        if cfg.connect_to is not None:
            logger.info(f"Meeting cluster at {cfg.connect_to}")
            messenger.meet(cfg.connect_to)
        with Context(messenger) as ctx:
            tsk.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            logger.info("launching executor")
            ctx.executor = Executor(ctx.messenger.recv, cfg)
            if cfg.run is not None:
                logger.info(f"with task {cfg.run}")
            ctx.executor.start(cfg.run)


class SubprocessWorker:
    def __init__(self, cfg):
        self.cfg = cfg
        self.p = multiprocessing.Process(target=zmq_launcher, args=(cfg,))

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # TODO: softer exit?
        self.p.terminate()
        self.p.join()


class BlockingWorker:
    def __init__(self, cfg):
        self.cfg = cfg

    def start(self):
        zmq_launcher(self.cfg)


def zmq_run(*, cfg=None, f=None):
    async def f_wrapper():
        result = await tsk.ctx().executor.wait_for_work(f)
        tsk.ctx().executor.stop = True
        f_wrapper.result = result

    f_wrapper.result = None

    if cfg is None:
        cfg = Cfg()
    cfg._build()
    with ExitStack() as es:
        workers = []
        main_cfg = cfg.get_worker_cfg(0)
        main_cfg.run = f_wrapper
        workers.append(BlockingWorker(main_cfg))
        for i in range(1, cfg.n_workers):
            workers.append(
                es.enter_context(SubprocessWorker(cfg.get_worker_cfg(i)))
            )
        workers[0].start()
    return f_wrapper.result

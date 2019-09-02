from contextlib import ExitStack, contextmanager
import multiprocessing
import asyncio

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
        #TODO: addr[1] = port, this is insufficient eventually, use uuid?
        #TODO: move ZMQClient/ZMQWorker to their own file
        messenger = JoinMeetMessenger(addr[1], comm, True)
        if meet_addr is not None:
            messenger.meet(meet_addr)
        with Context(messenger, cfg) as ctx:
            taskloaf.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            ctx.executor = Executor(ctx.messenger.recv, cfg, ctx.log)
            ctx.executor.start()

class ZMQWorker:
    def __init__(self, addr, cpu_affinity, meet_addr = None):
        self.addr = addr
        self.p = multiprocessing.Process(
            target = zmq_launcher,
            args = (addr, cpu_affinity, meet_addr)
        )

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        #TODO: softer exit?
        self.p.terminate()
        self.p.join()

class ZMQClient:
    def __init__(self, addr):
        self.addr = addr

    def __enter__(self):
        self.exit_stack = ExitStack()
        comm = self.exit_stack.enter_context(ZMQComm(
            self.addr,
            # [self.meet_addr],
        ))
        #TODO: move ZMQClient/ZMQWorker to their own file
        #TODO: addr[1] = port, this is insufficient eventually, use uuid?
        messenger = JoinMeetMessenger(self.addr[1], comm, False)
        self.ctx = self.exit_stack.enter_context(Context(
            messenger,
            dict()
        ))
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def update_cluster_view(self, addr = None):
        if addr is None:
            addr = next(iter(self.messenger.endpts.values()))[1]

        self.ctx.messenger.meet(addr)
        asyncio.get_event_loop().run_until_complete(
            self.ctx.messenger.recv()
        )

@contextmanager
def zmq_cluster(n_workers, hostname, ports):
    with ExitStack() as es:
        workers = []
        for i in range(n_workers):
            port = ports[i]
            meet_addr = None
            if i > 0:
                meet_addr = (hostname, ports[0])
            workers.append(es.enter_context(ZMQWorker(
                (hostname, port), [i], meet_addr = meet_addr
            )))
        yield workers

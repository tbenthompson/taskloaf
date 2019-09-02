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
        messenger.protocol.add_msg_type('COMPLETE', handler = lambda args: None)
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
        self.submissions = dict()

    def __enter__(self):
        self.exit_stack = ExitStack()
        comm = self.exit_stack.enter_context(ZMQComm(
            self.addr,
            # [self.meet_addr],
        ))
        #TODO: move ZMQClient/ZMQWorker to their own file
        #TODO: addr[1] = port, this is insufficient eventually, use uuid?
        self.messenger = JoinMeetMessenger(self.addr[1], comm, False)
        self.setup_complete_protocol()

        self.ctx = self.exit_stack.enter_context(Context(
            self.messenger,
            dict()
        ))
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def setup_complete_protocol(self):
        self.messenger.protocol.add_msg_type(
            'COMPLETE', handler = self.handle_complete
        )

    def update_cluster_view(self, addr = None):
        if addr is None:
            addr = next(iter(self.messenger.endpts.values()))[1]

        self.ctx.messenger.meet(addr)
        asyncio.get_event_loop().run_until_complete(
            self.ctx.messenger.recv()
        )
        self.cluster_names = list(self.ctx.messenger.endpts.keys())
        print('have endpts!', self.cluster_names)

    def submit(self, f, to = None):
        out_submission = Submission()
        submission_id = out_submission.id
        self.submissions[submission_id] = out_submission

        client_name = self.ctx.name
        async def submission_wrapper():
            result = await f()
            ctx = taskloaf.ctx()
            ctx.messenger.send(
                client_name,
                ctx.protocol.COMPLETE,
                (submission_id, result)
            )
        if to is None:
            to = self.cluster_names[0]
        self.ctx.messenger.send(
            to,
            self.ctx.messenger.protocol.WORK,
            submission_wrapper
        )
        return out_submission

    def handle_complete(self, args):
        submission_id, result = args
        self.submissions[submission_id].complete(result)

    def wait(self, submission):
        async def f():
            while True:
                await self.ctx.messenger.recv()
                if self.submissions[submission.id].is_complete:
                    break
        asyncio.get_event_loop().run_until_complete(f())
        del self.submissions[submission.id]
        return submission.result

class Submission:
    next_id = 0

    def __init__(self):
        self.id = Submission.next_id
        self.is_complete = False
        self.result = None
        Submission.next_id += 1

    def complete(self, result):
        self.is_complete = True
        self.result = result

default_base_port = 5754

@contextmanager
def zmq_client(
        connect_to,
        hostname = 'tcp://127.0.0.1',
        port = None):

    if port is None:
        port = default_base_port

    if type(connect_to) is ZMQCluster:
        connect_to = connect_to.addr()

    with ZMQClient((hostname, port)) as client:
        client.update_cluster_view(connect_to)
        yield client

class ZMQCluster:
    def __init__(self, workers):
        self.workers = workers

    def addr(self):
        return self.workers[0].addr


@contextmanager
def zmq_cluster(n_workers=None, hostname='tcp://127.0.0.1', ports=None):
    if n_workers is None:
        n_workers = psutil.cpu_count(logical = False)
    if ports is None:
        ports = range(default_base_port + 1, default_base_port + n_workers + 1)
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
        yield ZMQCluster(workers)

def run(f, n_workers=None, hostname='tcp://127.0.0.1', ports=None):
    if ports is None:
        client_port = None
        cluster_ports = None
    else:
        client_port = ports[0]
        cluster_ports = ports[1:]
    with zmq_cluster(
            n_workers = n_workers,
            hostname = hostname,
            ports = cluster_ports) as cluster:
        with zmq_client(
                cluster,
                hostname = hostname,
                port = client_port) as client:
            submission = client.submit(f)
            return client.wait(submission)

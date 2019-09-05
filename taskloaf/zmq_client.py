from contextlib import ExitStack, contextmanager
import asyncio

import taskloaf
from taskloaf.zmq_comm import ZMQComm
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.context import Context

from taskloaf.zmq_cluster import ZMQCluster


class ZMQClient:
    def __init__(self, addr):
        self.addr = addr
        self.submissions = dict()

    def __enter__(self):
        self.exit_stack = ExitStack()
        self.comm = self.exit_stack.enter_context(
            ZMQComm(
                self.addr,
                # [self.meet_addr],
            )
        )
        # TODO: move ZMQClient/ZMQWorker to their own file
        # TODO: addr[1] = port, this is insufficient eventually, use uuid?
        self.messenger = JoinMeetMessenger(self.addr[1], self.comm, False)
        self.setup_complete_protocol()

        self.ctx = self.exit_stack.enter_context(
            Context(self.messenger, dict())
        )
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def setup_complete_protocol(self):
        self.messenger.protocol.add_msg_type(
            "COMPLETE", handler=self.handle_complete
        )

    def update_cluster(self, addr=None):
        if addr is None:
            addr = next(iter(self.messenger.endpts.values()))[1]

        self.ctx.messenger.meet(addr)

        async def f():
            await self.ctx.messenger.recv()
            while True:
                if await self.comm.poll():
                    await self.ctx.messenger.recv()
                else:
                    break

        asyncio.get_event_loop().run_until_complete(f())

        # asyncio.get_event_loop().run_until_complete(self.ctx.messenger.recv())
        self.worker_names = list(self.ctx.messenger.endpts.keys())
        print("have endpts!", self.worker_names)

    def submit(self, f, to=None):
        out_submission = Submission()
        submission_id = out_submission.id
        self.submissions[submission_id] = out_submission

        client_name = self.ctx.name
        client_addr = self.addr

        async def submission_wrapper():
            result = await f()
            ctx = taskloaf.ctx()
            ctx.messenger.send(
                client_name,
                ctx.protocol.COMPLETE,
                (submission_id, result),
                connect_addr=client_addr,
            )

        if to is None:
            to = self.worker_names[0]
        self.ctx.messenger.send(
            to, self.ctx.messenger.protocol.WORK, submission_wrapper
        )
        return out_submission

    def handle_complete(self, args):
        submission_id, result = args
        self.submissions[submission_id].complete(result)
        del self.submissions[submission_id]

    def wait(self, submission):
        if not submission.is_complete:

            async def f():
                while True:
                    await self.ctx.messenger.recv()
                    if submission.is_complete:
                        break

            asyncio.get_event_loop().run_until_complete(f())

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


@contextmanager
def zmq_client(connect_to, hostname="tcp://127.0.0.1", port=None):

    if port is None:
        port = taskloaf.default_base_port

    if type(connect_to) is ZMQCluster:
        connect_to = connect_to.addr()

    with ZMQClient((hostname, port)) as client:
        client.update_cluster(connect_to)
        yield client

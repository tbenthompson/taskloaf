import time
import asyncio
import logging
import structlog
from contextlib import suppress, ExitStack

import taskloaf.protocol

# A NullComm is handy for running single-threaded or for some testing purposes
class NullComm:
    def __init__(self):
        self.addr = 0

    def send(self, to, data):
        pass

    def recv(self):
        return None

def shutdown(w):
    w.stop = True

class Worker:
    def __init__(self, comm):
        self.comm = comm
        self.init_time = time.time()
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()

        log_name = 'taskloaf.worker' + str(self.addr)
        self.log = structlog.wrap_logger(logging.getLogger(log_name))

        def handle_new_work(w, x):
            w.work.append(x[0])
        self.protocol.add_msg_type('WORK', handler = handle_new_work)
        self.exception = None
        self.next_id = 0
        self.stop = False

    def shutdown_all(self, addrs):
        for a in addrs:
            self.submit_work(a, shutdown)

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def __enter__(self):
        self.exit_stack = ExitStack()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def start(self, coro):
        main_task = asyncio.ensure_future(coro(self))
        self.ioloop = asyncio.get_event_loop()
        self.ioloop.run_until_complete(asyncio.gather(
            self.poll_loop(), self.work_loop(),
        ))

        pending = asyncio.Task.all_tasks()
        for task in pending:
            task.cancel()
            # Now we should await task to execute it's cancellation.
            # Cancelled task raises asyncio.CancelledError that we can suppress:
            with suppress(asyncio.CancelledError):
                self.ioloop.run_until_complete(task)

        if not main_task.cancelled() and main_task.exception():
            raise main_task.exception()

    @property
    def addr(self):
        return self.comm.addr

    def send(self, to, type_code, objs):
        data = self.protocol.encode(self, type_code, objs)
        self.comm.send(to, data)

    def submit_work(self, to, f):
        if to == self.addr:
            self.work.append(f)
        else:
            self.send(to, self.protocol.WORK, [f])

    def start_async_work(self, f, *args):
        return asyncio.ensure_future(f(self, *args))

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            self.start_async_work(f, *args)
        else:
            f(self, *args)

    async def wait_for_work(self, f, *args):
        out = f(self, *args)
        if asyncio.iscoroutinefunction(f):
            out = await out
        return out

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    async def work_loop(self):
        while not self.stop:
            if len(self.work) > 0:
                self.run_work(self.work.pop())
            await asyncio.sleep(0)

    def poll(self):
        msg = self.comm.recv()
        if msg is not None:
            m, args = self.protocol.decode(self, memoryview(msg))
            self.cur_msg = m
            self.protocol.handle(self, m.typeCode, args)
            self.cur_msg = None

    async def poll_loop(self):
        while not self.stop:
            self.poll()
            await asyncio.sleep(0)

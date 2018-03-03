import time
import asyncio
from concurrent.futures import CancelledError
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

async def shutdown(w):
    for t in asyncio.Task.all_tasks():
        t.cancel()
    await asyncio.sleep(0)
    for t in asyncio.Task.all_tasks():
        await t

class Worker:
    def __init__(self, comm):
        self.comm = comm
        self.st = time.time()
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()

        def handle_new_work(w, x):
            w.work.append(x[0])
        self.protocol.add_msg_type('WORK', handler = handle_new_work)
        self.exception = None
        self.next_id = 0

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def __enter__(self):
        self.exit_stack = ExitStack()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def start(self, coro):
        self.ioloop = asyncio.get_event_loop()
        with suppress(CancelledError):
            self.ioloop.run_until_complete(asyncio.gather(
                self.make_async_work(self.poll_loop),
                self.make_async_work(self.work_loop),
                self.make_async_work(coro)
            ))

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

    def make_async_work(self, f, *args):
        async def async_work_wrapper():
            with suppress(CancelledError):
                return await f(self, *args)
        return async_work_wrapper()

    def start_async_work(self, f, *args):
        return asyncio.ensure_future(self.make_async_work(f, *args))

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

    async def work_loop(self, _):
        while True:
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

    async def poll_loop(self, _):
        while True:
            self.poll()
            await asyncio.sleep(0)

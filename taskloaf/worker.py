import time
import asyncio
from concurrent.futures import CancelledError
from contextlib import suppress

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
    for t in asyncio.Task.all_tasks():
        t.cancel()

class Worker:
    def __init__(self, comm):
        self.comm = comm
        self.st = time.time()
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()
        self.protocol.add_msg_type('WORK', handler = lambda w, x: x[0])
        self.exception = None

    def start(self, coro):
        self.ioloop = asyncio.get_event_loop()

        async def coro_wrapper(w):
            self.result = await coro(w)

        self.result = None
        with suppress(CancelledError):
            self.ioloop.run_until_complete(asyncio.gather(
                self.poll_loop(),
                self.work_loop(),
                self.make_free_task(coro_wrapper, [])
            ))

        if self.exception is not None:
            # print('Free task experienced an exception, re-raising from addr =', self.addr)
            raise self.exception
        return self.result

    @property
    def addr(self):
        return self.comm.addr

    def schedule_work_here(self, type_code, args):
        self.work.append(self.protocol.handle(self, type_code, args))

    def send(self, to, type_code, objs):
        if self.addr == to:
            self.schedule_work_here(type_code, objs)
        else:
            data = self.protocol.encode(self, type_code, objs)
            self.comm.send(to, data)

    def submit_work(self, to, f):
        self.send(to, self.protocol.WORK, [f])

    def make_free_task(self, f, args):
        # TODO: DOCUMENT THIS. Old thoughts: How to catch exceptions that
        # happen in these functions?  They should catch their own exceptions
        # and stop the worker and set a flag with the exception. Then the
        # worker will raise a AsyncTaskException or something like that, and
        # print the subsidiary
        async def free_task_wrapper():
            try:
                with suppress(CancelledError):
                    return await f(self, *args)
            except Exception as e:
                self.exception = e
                self.submit_work(self.addr, shutdown)
        return free_task_wrapper()

    def start_free_task(self, f, args):
        return asyncio.ensure_future(self.make_free_task(f, args))

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            self.start_free_task(f, args)
        else:
            f(self, *args)

    async def wait_for_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            return await self.start_free_task(f, args)
        else:
            return f(self, *args)

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    async def work_loop(self):
        with suppress(CancelledError):
            while True:
                if len(self.work) > 0:
                    self.run_work(self.work.pop())
                await asyncio.sleep(0)

    def poll(self):
        msg = self.comm.recv()
        if msg is not None:
            m, args = self.protocol.decode(self, memoryview(msg))
            self.cur_msg = m
            self.schedule_work_here(m.typeCode, args)
            self.cur_msg = None

    async def poll_loop(self):
        with suppress(CancelledError):
            while True:
                self.poll()
                await asyncio.sleep(0)

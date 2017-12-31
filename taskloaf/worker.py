import time
import asyncio

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
    w.running = False
    # Can leaked async tasks be stopped somehow?
    # w.ioloop.stop()

class Worker:
    def __init__(self, comm):
        self.comm = comm
        self.st = time.time()
        self.running = None
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()
        self.protocol.add_msg_type('WORK', handler = lambda w, x: x[0])

    def start(self, coro):
        self.running = True

        # Should taskloaf create its own event loop?
        self.ioloop = asyncio.get_event_loop()
        results = self.ioloop.run_until_complete(asyncio.gather(
            self.poll_loop(), self.work_loop(), coro(self)
        ))

        # TODO: Wait for unfinished tasks?
        # pending = asyncio.Task.all_tasks()
        # loop.run_until_complete(asyncio.gather(*pending))

        assert(not self.running)
        return results[2]

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

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            # TODO: How to catch exceptions that happen in these functions?
            # They should catch their own exceptions and stop the worker and
            # set a flag with the exception. Then the worker will raise a
            # AsyncTaskException or something like that, and print the
            # subsidiary
            asyncio.ensure_future(f(self, *args))
        else:
            f(self, *args)

    async def wait_for_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            return await asyncio.ensure_future(f(self, *args))
        else:
            return f(self, *args)

    async def work_loop(self):
        while self.running:
            if len(self.work) > 0:
                self.run_work(self.work.pop())
            await asyncio.sleep(0)

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    def poll(self):
        msg = self.comm.recv()
        if msg is not None:
            m, args = self.protocol.decode(self, memoryview(msg))
            self.cur_msg = m
            self.schedule_work_here(m.typeCode, args)
            self.cur_msg = None

    async def poll_loop(self):
        while self.running:
            self.poll()
            await asyncio.sleep(0)

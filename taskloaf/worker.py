import time
import asyncio

import taskloaf.protocol

class NullComm:
    def __init__(self):
        self.addr = 0

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
        self.protocol.add_handler('WORK')

    def start(self, coro):
        self.running = True

        # Should taskloaf create its own event loop?
        self.ioloop = asyncio.get_event_loop()
        results = self.ioloop.run_until_complete(asyncio.gather(
            self.poll_loop(), self.work_loop(), coro(self)
        ))

        assert(not self.running)
        return results[2]

    @property
    def addr(self):
        return self.comm.addr

    def send(self, to, type_code, objs):
        if self.addr == to:
            self.work.append(self.protocol.build_work(type_code, objs))
        else:
            # start = time.time()
            data = self.protocol.encode(type_code, *objs)
            # T1 = time.time() - start
            # MB = sum([len(d) for d in data]) / 1e6
            # start = time.time()
            self.comm.send(to, data)
            # T2 = time.time() - start
            # if len(data) > 1e6:
            # print(
            #     'send', time.time() - self.st, self.protocol.get_name(type_code), 'from: ', self.addr,
            #     'to: ', to, ' MB: ', MB, (T1, T2)
            # )

    def submit_work(self, to, f):
        self.send(to, self.protocol.WORK, [f])

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
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
                # print('addr: ', services['comm'].addr, ' running work: ', len(work))
                self.run_work(self.work.pop())
            await asyncio.sleep(0)
        # print('quitting work loop(' + str(services['comm'].addr) + '): ' + str(len(work)))

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    def poll(self):
        msg = self.comm.recv()
        if msg is not None:
            # MB = sum([len(o) / 1e6 for o in objs])
            start = time.time()
            # print(objs)
            type_code, obj = self.protocol.decode(self, memoryview(msg))
            T = time.time() - start
            # print('recv', time.time() - self.st, self.protocol.get_name(type), 'to: ', self.addr, 'MB:', MB, T)
            self.work.append(self.protocol.build_work(type_code, obj))

    async def poll_loop(self):
        while self.running:
            self.poll()
            await asyncio.sleep(0)

import time
import inspect
import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

import taskloaf.protocol

def shutdown(w):
    w.running = False

# TODO: Should accessing Comm be allowed? Protocol?
class Worker:
    def __init__(self):
        self.st = time.time()
        self.running = None
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()
        self.WORK = self.protocol.add_handler('WORK')

    def start(self, comm, coros):
        self.running = True

        self.comm = comm
        self.ioloop = asyncio.get_event_loop()
        all_coros = [self.poll_loop(), self.work_loop()] + [c(self) for c in coros]

        results = self.ioloop.run_until_complete(asyncio.gather(*all_coros))
        return results[2:]

    @property
    def addr(self):
        return self.comm.addr

    def send(self, to, type_code, objs):
        if self.addr == to:
            self.work.append(self.protocol.build_work(type_code, objs))
        else:
            start = time.time()
            data = self.protocol.encode(type_code, objs)
            T1 = time.time() - start
            MB = sum([len(d) for d in data]) / 1e6
            start = time.time()
            self.comm.send(to, data)
            T2 = time.time() - start
            # if len(data) > 1e6:
            # print(
            #     'send', time.time() - self.st, self.protocol.get_name(type_code), 'from: ', self.addr,
            #     'to: ', to, ' MB: ', MB, (T1, T2)
            # )

    def submit_work(self, to, f):
        self.send(to, self.WORK, f)

    def run_work(self, f):
        if asyncio.iscoroutinefunction(f):
            return asyncio.ensure_future(f(self))
        else:
            return f(self)

    async def wait_for_work(self, f):
        result = self.run_work(f)
        if inspect.isawaitable(result):
            result = await result
        return result

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
        objs = self.comm.recv()
        if objs is not None:
            MB = sum([len(o) / 1e6 for o in objs])
            start = time.time()
            type, obj = self.protocol.decode(self, objs)
            T = time.time() - start
            # print('recv', time.time() - self.st, self.protocol.get_name(type), 'to: ', self.addr, 'MB:', MB, T)
            self.work.append(self.protocol.build_work(type, obj))

    async def poll_loop(self):
        while self.running:
            self.poll()
            await asyncio.sleep(0)

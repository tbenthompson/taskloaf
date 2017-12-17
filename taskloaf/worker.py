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
        self.running = None
        self.work = []
        self.protocol = taskloaf.protocol.Protocol()
        self.WORK = self.protocol.add_handler()

    def start(self, comm, coros):
        self.running = True

        self.comm = comm
        self.ioloop = asyncio.get_event_loop()
        all_coros = [self.comm_poll(), self.work_loop()] + [c(self) for c in coros]

        results = self.ioloop.run_until_complete(asyncio.gather(*all_coros))
        return results[2:]

    @property
    def addr(self):
        return self.comm.addr

    def send(self, to, type, obj):
        if self.addr == to:
            self.work.append(self.protocol.build_work(type, obj))
        else:
            self.comm.send(to, self.protocol.encode(type, obj))

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

    async def comm_poll(self):
        while self.running:
            t = self.comm.recv()
            if t is not None:
                self.work.append(self.protocol.decode(self, t))
            await asyncio.sleep(0)

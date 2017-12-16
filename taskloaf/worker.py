import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

def shutdown(w):
    w.running = False

class Worker:
    def __init__(self):
        self.running = None
        self.tasks = []

    def start(self, comm, coros):
        self.running = True

        self.comm = comm
        self.ioloop = asyncio.get_event_loop()
        all_coros = [self.comm_poll(), self.task_loop()] + [c(self) for c in coros]

        results = self.ioloop.run_until_complete(asyncio.gather(*all_coros))
        return results[2:]

    def local_task(self, f):
        self.tasks.append(f)

    def submit_task(self, to, f):
        if self.comm.addr == to:
            self.local_task(f)
        else:
            self.comm.send(to, f)

    def _run_task(self, f):
        if asyncio.iscoroutinefunction(f):
            return asyncio.ensure_future(f(self))
        else:
            return f(self)

    async def task_loop(self):
        while self.running:
            if len(self.tasks) > 0:
                # print('addr: ', services['comm'].addr, ' running task: ', len(tasks))
                self._run_task(self.tasks.pop())
            await asyncio.sleep(0)
        # print('quitting task loop(' + str(services['comm'].addr) + '): ' + str(len(tasks)))

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    async def comm_poll(self):
        while self.running:
            t = self.comm.recv(self)
            if t is not None:
                self.tasks.append(t)
            await asyncio.sleep(0)

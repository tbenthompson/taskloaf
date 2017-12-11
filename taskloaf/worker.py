import time
import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

import taskloaf.profile
# taskloaf.profile.enable_profiling()

running = None
tasks = []
services = dict()
services['ioloop'] = asyncio.get_event_loop()

def get_service(name):
    return services[name]

def shutdown():
    global running
    running = False

def start_worker(c, start_coro = None):
    global running
    running = True

    services['launch_time'] = time.time()
    services['waiting_futures'] = dict()
    services['ioloop'] = asyncio.get_event_loop()
    services['comm'] = c

    coros = [task_loop(), c.comm_poll(tasks)]
    if start_coro is not None:
        coros.append(start_coro)

    results = services['ioloop'].run_until_complete(asyncio.gather(*coros))

    if c.addr == 0:
        taskloaf.profile.profile_stats()

    if start_coro is not None:
        return results[-1]

def local_task(f):
    tasks.append(f)

def submit_task(to, f):
    c = services['comm']
    if c.addr == to:
        local_task(f)
    else:
        c.send(to, f)

def _run_task(f):
    if asyncio.iscoroutinefunction(f):
        return asyncio.ensure_future(f())
    else:
        return f()

async def task_loop():
    while running:
        if len(tasks) > 0:
            # print('addr: ', services['comm'].addr, ' running task: ', len(tasks))
            _run_task(tasks.pop())
        await asyncio.sleep(0)
    # print('quitting task loop(' + str(services['comm'].addr) + '): ' + str(len(tasks)))

async def run_in_thread(sync_f):
    return (await asyncio.get_event_loop().run_in_executor(None, sync_f))

class NullComm:
    def __init__(self):
        self.addr = 0

    async def comm_poll(self, tasks):
        return

def run(coro):
    async def caller():
        result = await coro
        shutdown()
        return result
    return start_worker(NullComm(), caller())

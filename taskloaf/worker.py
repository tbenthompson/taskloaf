import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

running = None
tasks = []
services = dict()
services['ioloop'] = asyncio.get_event_loop()

def get_service(name):
    return services[name]

def shutdown():
    global running
    running = False
    print("SHUTDOWN")

def start_worker(c, start_coro = None):
    global running
    running = True

    start_registries()
    services['ioloop'] = asyncio.get_event_loop()
    services['comm'] = c

    coros = [task_loop()]
    if c is not None:
        coros.append(comm_poll(c))
    if start_coro is not None:
        coros.append(start_coro)

    results = services['ioloop'].run_until_complete(asyncio.gather(*coros))
    if start_coro is not None:
        return results[-1]

def local_task(f):
    tasks.append(f)

def submit_task(to, f):
    c = get_service('comm')
    if c.addr == to:
        local_task(f)
    else:
        c.send(to, f)

def _run_task(f):
    if asyncio.iscoroutinefunction(f):
        return spawn(f())
    else:
        return f()

async def task_loop():
    while running:
        if len(tasks) > 0:
            _run_task(tasks.pop())
        await asyncio.sleep(0)

async def comm_poll(c):
    while running:
        t = c.recv()
        if t is not None:
            tasks.append(t)
        await asyncio.sleep(0)

def start_registries():
    services['waiting_futures'] = dict()
    services['signals_registry'] = dict()
    services['data'] = dict()

def spawn(coro):
    return asyncio.ensure_future(coro)

async def run_in_thread(sync_f):
    result = await asyncio.get_event_loop().run_in_executor(None, sync_f)
    return result

def run(coro):
    async def caller():
        result = await coro
        shutdown()
        return result
    return start_worker(None, caller())

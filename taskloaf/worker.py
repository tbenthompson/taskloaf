# from line_profiler import LineProfiler
# lp = LineProfiler()
# import builtins
# builtins.__dict__['profile'] = lp
#
def profile(f):
    return f

import asyncio
import uvloop
asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

running = None
tasks = []
services = dict()
services['ioloop'] = asyncio.get_event_loop()

@profile
def get_service(name):
    return services[name]

@profile
def shutdown():
    global running
    running = False
    print("SHUTDOWN!")

@profile
def local_task(f):
    tasks.append(f)

@profile
def submit_task(to, f):
    c = get_service('comm')
    if c.addr == to:
        local_task(f)
    else:
        c.send(to, f)

@profile
async def task_loop():
    while running:
        if len(tasks) > 0:
            tasks.pop()()
        # else:
        await asyncio.sleep(0)

@profile
async def comm_poll(c):
    services['comm'] = c
    while running:
        t = c.recv()
        if t is not None:
            tasks.append(t)
        await asyncio.sleep(0)

@profile
def start_signals_registry():
    services['signals_registry'] = dict()
    services['data'] = dict()

@profile
def start_worker(c):
    global running
    running = True
    start_signals_registry()
    services['ioloop'] = asyncio.get_event_loop()
    services['ioloop'].run_until_complete(asyncio.gather(task_loop(), comm_poll(c)))
    services['ioloop'].close()
    # lp.print_stats()

@profile
def start_client(c):
    services['comm'] = c
    start_signals_registry()

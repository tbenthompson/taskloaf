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
    print("SHUTDOWN!")

def local_task(f):
    tasks.append(f)

def submit_task(to, f):
    c = get_service('comm')
    if c.addr == to:
        local_task(f)
    else:
        c.send(to, f)

async def task_loop():
    while running:
        if len(tasks) == 0:
            await asyncio.sleep(0)
            continue
        tasks.pop()()

async def comm_poll(c):
    services['comm'] = c
    while running:
        t = c.recv()
        if t is not None:
            tasks.append(t)
        await asyncio.sleep(0)

def start_signals_registry():
    services['signals_registry'] = dict()

def launch_worker(c):
    global running
    running = True
    start_signals_registry()
    services['ioloop'] = asyncio.get_event_loop()
    services['ioloop'].run_until_complete(asyncio.gather(task_loop(), comm_poll(c)))
    services['ioloop'].close()

def launch_client(c):
    services['comm'] = c
    start_signals_registry()

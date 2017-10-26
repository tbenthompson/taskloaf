import time
import asyncio
import inspect
import traceback

running = None
ioloop = asyncio.get_event_loop()
tasks = []
services = dict()

def get_service(name):
    return services[name]

def shutdown():
    global running
    running = False
    print("SHUTDOWN!")

def submit_task(t):
    tasks.append(t)

async def task_loop():
    while running:
        if len(tasks) == 0:
            await asyncio.sleep(0)
            continue
        tasks.pop()()

def _launch(s_launchers):
    global running
    running = True
    ioloop.run_until_complete(asyncio.gather(task_loop(), *s_launchers))
    ioloop.close()

async def comm_poll(c):
    services['comm'] = c
    while running:
        t = c.recv()
        if t is not None:
            tasks.append(t)
        await asyncio.sleep(0)

def start_signals_registry():
    services['signals_registry'] = dict()

def launch(c):
    start_signals_registry()
    _launch([comm_poll(c)])

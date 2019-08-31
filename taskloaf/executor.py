import time
import asyncio
import logging
import traceback
from contextlib import suppress, ExitStack

import taskloaf.protocol

def shutdown(e):
    e.stop = True

class Executor:
    def __init__(self, recv_fnc, cfg, log):
        self.recv_fnc = recv_fnc
        self.cfg = cfg
        self.log = log
        self.init_time = time.time()
        self.stop = False
        self.work = []

    def start(self, coro = None):
        #TODO: Instead can I redesign to interop nicely with other event loops? Is that a good idea? No. Only necessary in the Client!
        self.ioloop = asyncio.get_event_loop()

        if coro is not None:
            main_task = asyncio.ensure_future(coro(self))

        start_task = asyncio.gather(
            self.poll_loop(), self.work_loop(),
            loop = self.ioloop
        )
        self.log.info('starting worker ioloop')
        self.ioloop.run_until_complete(start_task)

        pending = asyncio.Task.all_tasks(loop = self.ioloop)
        for task in pending:
            task.cancel()
            # Now we should await task to execute it's cancellation.
            # Cancelled task raises asyncio.CancelledError that we can suppress:
            with suppress(asyncio.CancelledError):
                self.ioloop.run_until_complete(task)

        if not main_task.cancelled() and main_task.exception():
            raise main_task.exception()

    def start_async_work(self, f, *args):
        async def async_work_wrapper():
            try:
                await f(*args)
            except asyncio.CancelledError:
                self.log.warning('async work cancelled', exc_info = True)
            except Exception as e:
                self.log.warning('async work failed with unhandled exception')
        return asyncio.ensure_future(
            async_work_wrapper(),
            loop = self.ioloop
        )

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            self.start_async_work(f, *args)
        else:
            # No need to catch exceptions here because this this is run
            # synchronously and the exception will bubble up to the owning
            # thread or task
            f(*args)

    async def wait_for_work(self, f, *args):
        out = f(self, *args)
        if asyncio.iscoroutinefunction(f):
            out = await out
        return out

    async def run_in_thread(self, sync_f):
        return (await self.ioloop.run_in_executor(None, sync_f))

    async def work_loop(self):
        while not self.stop:
            if len(self.work) > 0:
                try:
                    self.run_work(self.work.pop())
                except Exception as e:
                    self.log.warning('work failed with unhandled exception')
                    traceback.print_exc()
            await asyncio.sleep(0, loop = self.ioloop)

    async def poll_loop(self):
        while not self.stop:
            self.recv_fnc()
            await asyncio.sleep(0, loop = self.ioloop)
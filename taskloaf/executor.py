import time
import asyncio
from contextlib import suppress

import logging

logger = logging.getLogger(__name__)


def shutdown(e):
    e.stop = True


class Executor:
    def __init__(self, recv_fnc, cfg):
        self.recv_fnc = recv_fnc
        self.cfg = cfg
        self.init_time = time.time()
        self.stop = False
        self.work = asyncio.LifoQueue()

    def add_work(self, w):
        self.work.put_nowait(w)

    def start(self):
        # TODO: Instead can I redesign to interop nicely with other event
        # loops?  Is that a good idea? No. Only necessary in the Client!
        self.ioloop = asyncio.get_event_loop()

        start_task = asyncio.gather(
            self.poll_loop(), self.work_loop(), loop=self.ioloop
        )
        logger.info("starting worker ioloop")
        self.ioloop.run_until_complete(start_task)

        pending = asyncio.Task.all_tasks(loop=self.ioloop)
        for task in pending:
            task.cancel()
            # Now we should await task to execute it's cancellation.  Cancelled
            # task raises asyncio.CancelledError that we can suppress:
            with suppress(asyncio.CancelledError):
                self.ioloop.run_until_complete(task)

    def start_async_work(self, f, *args):
        async def async_work_wrapper():
            try:
                await f(*args)
            except asyncio.CancelledError:
                logger.warning("async work cancelled", exc_info=True)
            except Exception:
                logger.exception("async work failed with unhandled exception")

        return asyncio.ensure_future(async_work_wrapper(), loop=self.ioloop)

    def run_work(self, f, *args):
        if asyncio.iscoroutinefunction(f):
            self.start_async_work(f, *args)
        else:
            # No need to catch exceptions here because this this is run
            # synchronously and the exception will bubble up to the owning
            # thread or task
            f(*args)

    async def wait_for_work(self, f, *args):
        out = f(*args)
        if asyncio.iscoroutinefunction(f):
            out = await out
        return out

    async def run_in_thread(self, sync_f):
        return await self.ioloop.run_in_executor(None, sync_f)

    async def work_loop(self):
        while not self.stop:
            w = await self.work.get()
            try:
                self.run_work(w)
            except Exception:
                logger.exception("work failed with unhandled exception")

    async def poll_loop(self):
        while not self.stop:
            await self.recv_fnc()

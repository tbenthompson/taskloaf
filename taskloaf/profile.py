import taskloaf as tsk

import logging

logger = logging.getLogger(__name__)

# An example of a taskloaf "daemon"? Could be cool!


def start_profiler(w):
    import pyinstrument

    profiler = pyinstrument.Profiler()
    profiler.start()
    return profiler


def stop_profiler(w, profiler):
    profiler.stop()
    if w is not None:
        logger.info(f"profile for addr: {w.addr}")
    logger.info(profiler.output_text(unicode=True, color=True))


class Profiler:
    def __init__(self, w, addrs):
        self.w = w
        self.addrs = addrs

    async def start(self):
        self.prof_tasks = [
            tsk.task(self.w, start_profiler, to=addr) for addr in self.addrs
        ]
        for pt in self.prof_tasks:
            await pt

    async def stop(self):
        for i in range(len(self.prof_tasks)):
            await self.prof_tasks[i].then(stop_profiler, to=self.addrs[i])

    async def __aenter__(self):
        await self.start()

    async def __aexit__(self, *args):
        await self.stop()

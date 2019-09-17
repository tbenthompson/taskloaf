import taskloaf as tsk

import logging

logger = logging.getLogger(__name__)

# An example of a taskloaf "daemon"? Could be cool!


def start_profiler():
    import pyinstrument

    profiler = pyinstrument.Profiler()
    profiler.start()
    return profiler


def stop_profiler(profiler):
    profiler.stop()
    logger.info(f"profile for name: {tsk.ctx().name}")
    logger.info(profiler.output_text(unicode=True, color=True))


class Profiler:
    def __init__(self, names=None):
        if names is None:
            names = tsk.ctx().get_all_names()
        self.names = names

    async def start(self):
        self.prof_tasks = [tsk.task(start_profiler, to=n) for n in self.names]
        for pt in self.prof_tasks:
            await pt

    async def stop(self):
        for i in range(len(self.prof_tasks)):
            await self.prof_tasks[i].then(stop_profiler, to=self.names[i])

    async def __aenter__(self):
        await self.start()

    async def __aexit__(self, *args):
        await self.stop()

import asyncio
from taskloaf.worker import submit_task, get_service
from uuid import uuid1 as uuid

def new_id(addr):
    return uuid(addr)

def complete_task(S, result):
    wf = get_service('waiting_futures')
    if S not in wf:
        wf[S] = asyncio.Future()
    wf[S].set_result(result)

class Promise:
    def __init__(self, owner, signal):
        self.owner = owner
        self.signal = signal

    def __await__(self):
        here = get_service('comm').addr
        waiting_futures = get_service('waiting_futures')
        if self.signal not in waiting_futures:
            waiting_futures[self.signal] = asyncio.Future()
        if here != self.owner:
            async def wait_for_promise():
                v = await self
                submit_task(here, lambda: get_service('waiting_futures')[self.signal].set_result(v))
            submit_task(self.owner, wait_for_promise)
        return waiting_futures[self.signal].__await__()

    def then(self, f, to = None):
        if to is None:
            to = self.owner
        S = new_id(to)

        async def wait_to_start():
            v = await self
            task(lambda v=v: f(v), to = to, S = S)
        submit_task(self.owner, wait_to_start)

        return Promise(to, S)

    def next(self, f, to = None):
        return self.then(lambda x: f(), to)

def task(f, to = None, S = None):
    if to is None:
        to = get_service('comm').addr

    if S is None:
        S = new_id(to)

    def work_wrapper():
        assert(get_service('comm').addr == to)
        result = f()
        if isinstance(result, Promise):
            result.then(lambda x: complete_task(S, x))
        else:
            complete_task(S, result)
    submit_task(to, work_wrapper)
    return Promise(to, S)

def when_all(ps, to = None):
    if to is None:
        to = ps[0].owner

    S = new_id(to)
    n = len(ps)
    async def wait_for_all():
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        complete_task(S, results)
    submit_task(to, wait_for_all)
    return Promise(to, S)

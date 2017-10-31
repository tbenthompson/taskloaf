import asyncio
import inspect
from taskloaf.worker import submit_task, get_service
from uuid import uuid1 as uuid

def _new_id(addr):
    return uuid(addr)

class Promise:
    def __init__(self, owner, id_):
        self.owner = owner
        self.id_ = id_

    def __await__(self):
        here = get_service('comm').addr
        wf = get_service('waiting_futures')
        if self.id_ not in wf:
            wf[self.id_] = asyncio.Future()
        if here != self.owner:
            async def wait_for_promise():
                v = await self
                submit_task(here, lambda: get_service('waiting_futures')[self.id_].set_result(v))
            submit_task(self.owner, wait_for_promise)
        return wf[self.id_].__await__()

    def set_result(self, result):
        wf = get_service('waiting_futures')
        if self.id_ not in wf:
            wf[self.id_] = asyncio.Future()
        wf[self.id_].set_result(result)

    def then(self, f, to = None):
        if to is None:
            to = self.owner
        id_ = _new_id(to)

        async def wait_to_start():
            v = await self
            task(lambda v=v: f(v), to = to, id_ = id_)
        submit_task(self.owner, wait_to_start)

        return Promise(to, id_)

    def next(self, f, to = None):
        return self.then(lambda x: f(), to)

def _unwrap_promise(pr, result):
    if isinstance(result, Promise):
        result.then(pr.set_result)
    else:
        pr.set_result(result)

def task(f, to = None, id_ = None):
    if to is None:
        to = get_service('comm').addr

    if id_ is None:
        id_ = _new_id(to)

    out_pr = Promise(to, id_)

    async def work_wrapper():
        assert(get_service('comm').addr == to)
        result = f()
        if inspect.isawaitable(result):
            result = await result
        _unwrap_promise(out_pr, result)

    submit_task(to, work_wrapper)
    return out_pr

def when_all(ps, to = None):
    if to is None:
        to = ps[0].owner

    id_ = _new_id(to)
    out_pr = Promise(to, id_)

    n = len(ps)
    async def wait_for_all():
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        out_pr.set_result(results)
    submit_task(to, wait_for_all)
    return out_pr

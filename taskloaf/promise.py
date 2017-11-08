import asyncio
import inspect
from taskloaf.worker import local_task, submit_task, get_service
from uuid import uuid1 as uuid

def _ensure_future_exists(id_, owner):
    wf = get_service('waiting_futures')
    if id_ not in wf:
        here = get_service('comm').addr
        if here != owner:
            wf[id_] = [asyncio.Future(), False, owner]
            async def delete_after_triggered():
                await wf[id_][0]
                del wf[id_]
            local_task(delete_after_triggered)
        else:
            wf[id_] = [asyncio.Future(), [1], owner]

class Promise:
    def __init__(self, owner):
        self.owner = owner
        self.id_ = uuid(self.owner)
        self.gen = 0
        self.n_children = 0

    def __getstate__(self):
        self.n_children += 1
        return dict(
            gen = self.gen + 1,
            n_children = 0,
            owner = self.owner,
            id_ = self.id_,
        )

    def __del__(self):
        def dec_ref(d):
            id_, gen, n_children = d
            wf = get_service('waiting_futures')
            _ensure_future_exists(self.id_, self.owner)
            gen_counts = wf[id_][1]
            if len(gen_counts) < gen + 2:
                gen_counts.extend([0] * (gen + 2 - len(gen_counts)))
            gen_counts[gen] -= 1
            gen_counts[gen + 1] += n_children
            for c in gen_counts:
                if c != 0:
                    return
            del wf[id_]
        data = (self.id_, self.gen, self.n_children)
        submit_task(self.owner, lambda d=data: dec_ref(d))

    def __await__(self):
        here = get_service('comm').addr
        wf = get_service('waiting_futures')
        _ensure_future_exists(self.id_, self.owner)
        if here != self.owner and not wf[self.id_][1]:
            wf[self.id_][1] = True
            async def wait_for_promise():
                v = await self
                submit_task(here, lambda: self.set_result(v))
            submit_task(self.owner, wait_for_promise)
        return wf[self.id_][0].__await__()

    def set_result(self, result):
        _ensure_future_exists(self.id_, self.owner)
        get_service('waiting_futures')[self.id_][0].set_result(result)

    def then(self, f, to = None):
        if to is None:
            to = self.owner

        out_pr = Promise(to)
        async def wait_to_start():
            v = await self
            task(lambda v=v: f(v), out_pr = out_pr)
        submit_task(self.owner, wait_to_start)
        return out_pr

    def next(self, f, to = None):
        return self.then(lambda x: f(), to)

def _unwrap_promise(pr, result):
    if isinstance(result, Promise):
        result.then(pr.set_result)
    else:
        pr.set_result(result)

def task(f, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = get_service('comm').addr
        out_pr = Promise(to)

    async def work_wrapper():
        assert(get_service('comm').addr == out_pr.owner)
        result = f()
        if inspect.isawaitable(result):
            result = await result
        _unwrap_promise(out_pr, result)

    submit_task(out_pr.owner, work_wrapper)
    return out_pr

def when_all(ps, to = None):
    if to is None:
        to = ps[0].owner
    out_pr = Promise(to)

    n = len(ps)
    async def wait_for_all():
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        out_pr.set_result(results)
    submit_task(out_pr.owner, wait_for_all)
    return out_pr

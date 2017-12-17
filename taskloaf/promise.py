import asyncio
from taskloaf.memory import Ref
from uuid import uuid1 as uuid

import pickle
import taskloaf.serialize

class PromiseManager:
    def __init__(self, w):
        self.TASK = w.protocol.add_handler(
            work_builder = task_runner_builder
        )
        self.SET_RESULT = w.protocol.add_handler(
            # encoder = set_result_encoder,
            # decoder = set_result_decoder,
            work_builder = set_result_builder
        )
        self.AWAIT = w.protocol.add_handler(
            work_builder = await_builder
        )

def set_result_encoder(args):
    return taskloaf.serialize.dumps((args[0], pickle.dumps(args[1])))

def set_result_decoder(w, b):
    pair = taskloaf.serialize.loads(w, b)
    return pair[0], pickle.loads(pair[1])

def set_result_builder(args):
    pr, v = args
    def run(w):
        pr.set_result(v)
    return run

def await_builder(args):
    pr, req_addr = args
    async def run(w):
        v = await pr
        w.send(req_addr, w.promise_manager.SET_RESULT, (pr, v))
    return run

class Promise:
    def __init__(self, w, owner):
        self.r = Ref(w, owner)
        self.ensure_future_exists()

    @property
    def w(self):
        return self.r.w

    @property
    def pm(self):
        return self.r.w.promise_manager

    @property
    def owner(self):
        return self.r.owner

    def ensure_future_exists(self):
        if not self.w.memory.available(self.r):
            here = self.w.comm.addr
            self.w.memory.put([asyncio.Future(), False], r = self.r)
            if self.owner != here:
                async def delete_after_triggered(w):
                    await w.memory.get(self.r)[0]
                    w.memory.delete(self.r)
                self.w.run_work(delete_after_triggered)

    def __await__(self):
        here = self.w.comm.addr
        self.ensure_future_exists()
        pr_data = self.w.memory.get(self.r)
        if here != self.owner and not pr_data[1]:
            pr_data[1] = True
            self.w.send(self.owner, self.w.promise_manager.AWAIT, (self, here))
        return pr_data[0].__await__()
        # return wf[self.id_][0].__await__()

    def set_result(self, result):
        self.ensure_future_exists()
        self.w.memory.get(self.r)[0].set_result(result)

    def then(self, f, to = None):
        if to is None:
            to = self.owner
        out_pr = Promise(self.w, to)

        async def wait_to_start(w):
            v = await self
            task(w, lambda w, v=v: f(w, v), out_pr = out_pr)
        self.w.submit_work(self.owner, wait_to_start)
        return out_pr

    def next(self, f, to = None):
        return self.then(lambda x: f(), to)

def _unwrap_promise(pr, result):
    if isinstance(result, Promise):
        result.then(pr.set_result)
    else:
        pr.set_result(result)

def task_runner_builder(args):
    f, out_pr = args
    async def task_runner(w):
        assert(w.comm.addr == out_pr.owner)
        _unwrap_promise(out_pr, await w.wait_for_work(f))
    return task_runner

def task(w, f, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = w.comm.addr
        out_pr = Promise(w, to)

    w.send(out_pr.owner, w.promise_manager.TASK, (f, out_pr))
    return out_pr

def when_all(ps, to = None):
    w = ps[0].w
    if to is None:
        to = ps[0].owner
    out_pr = Promise(w, to)

    n = len(ps)
    async def wait_for_all(w):
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        out_pr.set_result(results)
    w.submit_work(out_pr.owner, wait_for_all)
    return out_pr

async def remote_get(r):
    if not r.available():
        getter_addr = r.w.comm.addr
        async def remote_get(w):
            value = r.get()
            def remote_put(w, v):
                w.memory.put(v, r = r)
            return await task(w, lambda w, v = value: remote_put(w, v), to = getter_addr)
        await task(r.w, remote_get, to = r.owner)
    return r.get()

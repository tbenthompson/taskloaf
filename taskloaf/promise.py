import asyncio
import capnp

import taskloaf.serialize
from taskloaf.memory import DistributedRef, DRefListSerializer, remote_get

def setup_protocol(worker):
    worker.protocol.add_msg_type(
        'TASK',
        serializer = DRefListSerializer,
        handler = task_runner_builder
    )
    worker.protocol.add_msg_type(
        'SETRESULT',
        serializer = DRefListSerializer,
        handler = set_result_builder
    )
    worker.protocol.add_msg_type(
        'AWAIT',
        serializer = DRefListSerializer,
        handler = await_builder
    )

def await_builder(args):
    pr_dref, req_addr = args
    async def run(worker):
        res_dref = await Promise(pr_dref).await_result_dref()
        print(res_dref, pr_dref)
        worker.send(req_addr, worker.protocol.SETRESULT, [pr_dref, res_dref])
    return run

def set_result_builder(args):
    async def run(worker):
        fut_val = await remote_get(worker, args[1])
        worker.memory.put(fut_val, dref = args[1])
        Promise(args[0]).set_result(args[1])
    return run

class Promise:
    def __init__(self, dref):
        self.dref = dref
        self.ensure_future_exists()

    @property
    def worker(self):
        return self.dref.worker

    @property
    def owner(self):
        return self.dref.owner

    def ensure_future_exists(self):
        if not self.worker.memory.available(self.dref):
            here = self.worker.comm.addr
            self.worker.memory.put([asyncio.Future(), False], dref = self.dref)
            if self.owner != here:
                async def delete_after_triggered(worker):
                    await worker.memory.get(self.dref)[0]
                    worker.memory.delete(self.dref)
                self.worker.run_work(delete_after_triggered)

    def __await__(self):
        result_dref = yield from self.await_result_dref()
        out = yield from remote_get(self.worker, result_dref).__await__()
        return out

    def await_result_dref(self):
        here = self.worker.comm.addr
        self.ensure_future_exists()
        pr_data = self.worker.memory.get(self.dref)
        if here != self.owner and not pr_data[1]:
            pr_data[1] = True
            self.worker.send(self.owner, self.worker.protocol.AWAIT, [self.dref, here])
        return pr_data[0]

    def set_result(self, result):
        self.ensure_future_exists()
        res_dref = self.worker.memory.put(result)
        self.worker.memory.get(self.dref)[0].set_result(res_dref)

    def then(self, f, to = None):
        if to is None:
            to = self.owner
        out_pr = Promise(DistributedRef(self.worker, to))

        async def wait_to_start(worker):
            v = await self
            task(worker, lambda worker, v=v: f(worker, v), out_pr = out_pr)
        self.worker.submit_work(self.owner, wait_to_start)
        return out_pr

    def next(self, f, to = None):
        return self.then(lambda x: f(), to)

def _unwrap_promise(pr, result):
    if isinstance(result, Promise):
        result.then(lambda w, x: pr.set_result(x))
    else:
        pr.set_result(result)

def task_runner_builder(data):
    async def task_runner(worker):
        out_pr = Promise(data[0])
        assert(worker.comm.addr == out_pr.dref.owner)

        f = data[1]
        if is_dref(f):
            f = await remote_get(worker, f)

        if len(data) > 2:
            args = data[2]
            if is_dref(args):
                args = await remote_get(worker, args)
            waiter = worker.wait_for_work(f, args)
        else:
            waiter = worker.wait_for_work(f)
        _unwrap_promise(out_pr, await waiter)
    return task_runner

def is_dref(v):
    return isinstance(v, DistributedRef)

def ensure_dref_if_remote(worker, v, to):
    if worker.addr == to or is_dref(v):
        return v
    return worker.memory.put(v)

# f and args can be provided in two forms:
# -- a python object (f should be callable or awaitable)
# -- a dref to a serialized object in the memory manager
# if f is a function and the task is being run locally, f is never serialized, but when the task is being run remotely, f is entered into the
def task(worker, f, args = None, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = worker.comm.addr
        out_pr = Promise(DistributedRef(worker, to))

    task_objs = [
        out_pr.dref,
        ensure_dref_if_remote(worker, f, to)
    ]
    if args is not None:
        task_objs.append(ensure_dref_if_remote(worker, args, to))
    worker.send(out_pr.owner, worker.protocol.TASK, task_objs)

    return out_pr

def when_all(ps, to = None):
    worker = ps[0].worker
    if to is None:
        to = ps[0].owner
    out_pr = Promise(DistributedRef(worker, to))

    n = len(ps)
    async def wait_for_all(worker):
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        out_pr.set_result(results)
    worker.submit_work(out_pr.owner, wait_for_all)
    return out_pr
import asyncio
import capnp

import taskloaf.serialize
from taskloaf.memory import DistributedRef
from taskloaf.remote_get import get, DRefListSerializer

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

def await_builder(worker, args):
    pr_dref = args[0]
    req_addr = worker.cur_msg.sourceAddr
    async def run(worker):
        res_dref = await Promise(pr_dref).await_result_dref()
        worker.send(req_addr, worker.protocol.SETRESULT, [pr_dref, res_dref])
    return run

def set_result_builder(worker, args):
    async def run(worker):
        #TODO: Why were these lines necessary? I need to get some no-shmem
        # tests going so that I don't have to worry about designing something
        # that won't work distributed
        # fut_val = await get(worker, args[1])
        # worker.memory.put(value = fut_val, dref = args[1])
        worker.memory.get_local(args[0])[0].set_result(args[1])
    return run

class Promise:
    def __init__(self, dref):
        self.dref = dref

    @property
    def worker(self):
        return self.dref.worker

    @property
    def owner(self):
        return self.dref.owner

    def ensure_future_exists(self):
        if not self.worker.memory.available(self.dref):
            here = self.worker.comm.addr
            self.worker.memory.put(
                value = [asyncio.Future(), False],
                dref = self.dref
            )
            if self.owner != here:
                async def delete_after_triggered(worker):
                    await worker.memory.get_local(self.dref)[0]
                    worker.memory.delete(self.dref)
                self.worker.run_work(delete_after_triggered)

    def __await__(self):
        res_dref = yield from self.await_result_dref()
        out = yield from get(self.worker, res_dref).__await__()
        return out

    def await_result_dref(self):
        here = self.worker.comm.addr
        self.ensure_future_exists()
        pr_data = self.worker.memory.get_local(self.dref)
        if here != self.owner and not pr_data[1]:
            pr_data[1] = True
            self.worker.send(self.owner, self.worker.protocol.AWAIT, [self.dref])
        return pr_data[0]

    def set_result(self, result):
        self.ensure_future_exists()
        res_dref = self.worker.memory.put(value = result)
        self.worker.memory.get_local(self.dref)[0].set_result(res_dref)

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

def task_runner_builder(worker, data):
    async def task_runner(worker):
        out_pr = Promise(data[0])
        assert(worker.comm.addr == out_pr.dref.owner)

        f = data[1]
        if is_dref(f):
            f = await get(worker, f)

        if len(data) > 2:
            args = data[2]
            if is_dref(args):
                args = await get(worker, args)
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
    return worker.memory.put(value = v)

# BAD OLD COMMENT?
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

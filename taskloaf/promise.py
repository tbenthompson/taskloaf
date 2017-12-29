import asyncio
import capnp

from taskloaf.memory import DistributedRef, DRefListSerializer

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

def set_result_builder(args):
    pr, v = args
    def run(worker):
        pr.set_result(v)
    return run

def await_builder(args):
    pr, req_addr = args
    async def run(worker):
        v = await pr
        worker.send(req_addr, worker.protocol.SET_RESULT, [pr, v])
    return run

class Promise:
    def __init__(self, worker, owner):
        self.dref = DistributedRef(worker, owner)
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
        here = self.worker.comm.addr
        self.ensure_future_exists()
        pr_data = self.worker.memory.get(self.dref)
        if here != self.owner and not pr_data[1]:
            pr_data[1] = True
            self.worker.send(self.owner, self.worker.protocol.AWAIT, [self, here])
        return pr_data[0].__await__()
        # return wf[self.id_][0].__await__()

    def set_result(self, result):
        self.ensure_future_exists()
        self.worker.memory.get(self.dref)[0].set_result(result)

    def then(self, f, to = None):
        if to is None:
            to = self.owner
        out_pr = Promise(self.worker, to)

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
    print(data)
    f_b = data[0]
    out_pr = data[1]
    has_args = data[2]
    args_b = data[3]
    async def task_runner(worker):
        assert(worker.comm.addr == out_pr.owner)
        if isinstance(f_b, bytes):
            f = taskloaf.serialize.loads(worker, f_b)
        else:
            f = f_b

        if has_args:
            waiter = worker.wait_for_work(f, args_b)
        else:
            waiter = worker.wait_for_work(f)
        _unwrap_promise(out_pr, await waiter)
    return task_runner

def to_dref(worker, v):
    if isinstance(v, DistributedRef):
        return v
    return worker.memory.put(v)

def task(worker, f, args = None, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = worker.comm.addr
        out_pr = Promise(worker, to)

    drefs = [
        out_pr.dref,
        to_dref(worker, f)
    ]
    if args is not None:
        drefs.append(to_dref(worker, args))
    worker.send(out_pr.owner, worker.protocol.TASK, drefs)

    return out_pr

def when_all(ps, to = None):
    worker = ps[0].worker
    if to is None:
        to = ps[0].owner
    out_pr = Promise(worker, to)

    n = len(ps)
    async def wait_for_all(worker):
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        out_pr.set_result(results)
    worker.submit_work(out_pr.owner, wait_for_all)
    return out_pr

async def remote_get(dref):
    if not dref.available():
        dref.put(await task(dref.worker, lambda worker: dref.get(), to = dref.owner))
    return dref.get()

import asyncio
import capnp

from taskloaf.memory import DistributedRef
from taskloaf.serialize import encode_maybe_bytes, decode_maybe_bytes
import taskloaf.message_capnp

def setup_protocol(worker):
    worker.protocol.add_handler(
        'TASK',
        encoder = task_encoder,
        decoder = task_decoder,
        work_builder = task_runner_builder
    )
    worker.protocol.add_handler(
        'SET_RESULT',
        encoder = set_result_encoder,
        decoder = set_result_decoder,
        work_builder = set_result_builder
    )
    worker.protocol.add_handler(
        'AWAIT',
        encoder = await_encoder,
        decoder = await_decoder,
        work_builder = await_builder
    )
    worker.protocol.add_handler(
        'NEWTASK', work_builder = new_task_runner_builder
    )


def encode_ref(dest, ref):
    ref.n_children += 1
    dest.owner = ref.owner
    dest.creator = ref.creator
    dest.id = ref._id
    dest.gen = ref.gen + 1

def decode_promise(worker, capnp_ref):
    dref = DistributedRef.__new__(DistributedRef)
    dref.worker = worker
    dref.owner = capnp_ref.owner
    dref.creator = capnp_ref.creator
    dref._id = capnp_ref.id
    dref.gen = capnp_ref.gen
    dref.n_children = 0
    p = Promise.__new__(Promise)
    p.dref = dref
    return p

def set_result_encoder(type_code, pr, v):
    m = taskloaf.task_capnp.Message.new_message()
    m.typeCode = type_code
    m.init('setresult')
    encode_ref(m.setresult.ref, pr.dref)
    encode_maybe_bytes(m.setresult.v, v)
    return bytes(8) + m.to_bytes()

def set_result_decoder(worker, b):
    m = taskloaf.task_capnp.Message.from_bytes(b)
    return decode_promise(worker, m.setresult.ref), decode_maybe_bytes(worker, m.setresult.v),

def task_encoder(type_code, f, pr, has_args, args):
    m = taskloaf.task_capnp.Task.new_message()
    m.typeCode = type_code
    encode_ref(m.ref, pr.dref)
    encode_maybe_bytes(m.f, f)
    m.hasargs = has_args
    if has_args:
        encode_maybe_bytes(m.args, args)
    return m.to_bytes()

def task_decoder(worker, b):
    m = taskloaf.task_capnp.Task.from_bytes(b)
    args_b = None
    if m.hasargs:
        args_b = decode_maybe_bytes(worker, m.args)
    return (
        decode_maybe_bytes(worker, m.f),
        decode_promise(worker, m.ref),
        m.hasargs,
        args_b
    )

def await_encoder(type_code, pr, req_addr):
    m = taskloaf.task_capnp.Message.new_message()
    m.typeCode = type_code
    m.init('await')
    encode_ref(m.await.ref, pr.dref)
    m.await.reqaddr = req_addr
    return bytes(8) + m.to_bytes()

def await_decoder(worker, b):
    m = taskloaf.task_capnp.Message.from_bytes(b)
    return decode_promise(worker, m.await.ref), m.await.reqaddr

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

# TODO: once this new version works, clear out the old stuff
def new_task_runner_builder(data):
    pass

def to_dref(worker, v):
    if isinstance(v, DistributedRef):
        return v
    return worker.memory.put(v)

def task(worker, f, args = None, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = worker.comm.addr
        out_pr = Promise(worker, to)

    msg = [f, out_pr, args is not None, args]
    worker.send(out_pr.owner, worker.protocol.TASK, msg)

    # drefs = [
    #     out_pr.dref,
    #     to_dref(worker, f)
    # ]
    # if args is not None:
    #     drefs.append(to_dref(worker, args))
    # worker.new_send(out_pr.owner, worker.protocol.NEWTASK, drefs)

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

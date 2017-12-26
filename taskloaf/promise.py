import asyncio
import capnp

from taskloaf.memory import Ref
from taskloaf.serialize import encode_maybe_bytes, decode_maybe_bytes
import taskloaf.task_capnp

def setup_protocol(w):
    w.protocol.add_handler(
        'TASK',
        encoder = task_encoder,
        decoder = task_decoder,
        work_builder = task_runner_builder
    )
    w.protocol.add_handler(
        'SET_RESULT',
        encoder = set_result_encoder,
        decoder = set_result_decoder,
        work_builder = set_result_builder
    )
    w.protocol.add_handler(
        'AWAIT',
        encoder = await_encoder,
        decoder = await_decoder,
        work_builder = await_builder
    )


def encode_ref(dest, ref):
    ref.n_children += 1
    dest.owner = ref.owner
    dest.creator = ref.creator
    dest.id = ref._id
    dest.gen = ref.gen + 1

def decode_promise(w, capnp_ref):
    r = Ref.__new__(Ref)
    r.w = w
    r.owner = capnp_ref.owner
    r.creator = capnp_ref.creator
    r._id = capnp_ref.id
    r.gen = capnp_ref.gen
    r.n_children = 0
    p = Promise.__new__(Promise)
    p.r = r
    return p

def set_result_encoder(pr, v):
    m = taskloaf.task_capnp.Message.new_message()
    m.init('setresult')
    encode_ref(m.setresult.ref, pr.r)
    encode_maybe_bytes(m.setresult.v, v)
    return bytes(8) + m.to_bytes()

def set_result_decoder(w, b):
    m = taskloaf.task_capnp.Message.from_bytes(b)
    return decode_promise(w, m.setresult.ref), decode_maybe_bytes(w, m.setresult.v),

def task_encoder(f, pr, has_args, args):
    m = taskloaf.task_capnp.Message.new_message()
    m.init('task')
    encode_ref(m.task.ref, pr.r)
    encode_maybe_bytes(m.task.f, f)
    m.task.hasargs = has_args
    if has_args:
        encode_maybe_bytes(m.task.args, args)
    return bytes(8) + m.to_bytes()

def task_decoder(w, b):
    m = taskloaf.task_capnp.Message.from_bytes(b)
    args_b = None
    if m.task.hasargs:
        args_b = decode_maybe_bytes(w, m.task.args)
    return (
        decode_maybe_bytes(w, m.task.f),
        decode_promise(w, m.task.ref),
        m.task.hasargs,
        args_b
    )

def await_encoder(pr, req_addr):
    m = taskloaf.task_capnp.Message.new_message()
    m.init('await')
    encode_ref(m.await.ref, pr.r)
    m.await.reqaddr = req_addr
    return bytes(8) + m.to_bytes()

def await_decoder(w, b):
    m = taskloaf.task_capnp.Message.from_bytes(b)
    return decode_promise(w, m.await.ref), m.await.reqaddr

def set_result_builder(args):
    pr, v = args
    def run(w):
        pr.set_result(v)
    return run

def await_builder(args):
    pr, req_addr = args
    async def run(w):
        v = await pr
        w.send(req_addr, w.protocol.SET_RESULT, [pr, v])
    return run

class Promise:
    def __init__(self, w, owner):
        self.r = Ref(w, owner)
        self.ensure_future_exists()

    @property
    def w(self):
        return self.r.w

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
            self.w.send(self.owner, self.w.protocol.AWAIT, [self, here])
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

def task_runner_builder(data):
    f_b = data[0]
    out_pr = data[1]
    has_args = data[2]
    args_b = data[3]
    async def task_runner(w):
        assert(w.comm.addr == out_pr.owner)
        if isinstance(f_b, bytes):
            f = taskloaf.serialize.loads(w, f_b)
        else:
            f = f_b

        if has_args:
            waiter = w.wait_for_work(f, args_b)
        else:
            waiter = w.wait_for_work(f)
        _unwrap_promise(out_pr, await waiter)
    return task_runner

def task(w, f, args = None, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = w.comm.addr
        out_pr = Promise(w, to)

    msg = [f, out_pr, args is not None, args]
    w.send(out_pr.owner, w.protocol.TASK, msg)
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
        r.put(await task(r.w, lambda w: r.get(), to = r.owner))
    return r.get()

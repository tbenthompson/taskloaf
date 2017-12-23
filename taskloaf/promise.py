import asyncio
from taskloaf.memory import Ref
import uuid
import struct

import taskloaf.serialize
from io import BytesIO

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


def struct_pack(fmt, *args):
    out = memoryview(bytearray(struct.calcsize(fmt) + 8))
    struct.pack_into(fmt, out, 8, *args)
    return out

def unpack_promise(w, vals, offset):
    r = Ref.__new__(Ref)
    r.w = w
    r.owner = vals[offset]
    r.creator = vals[offset+1]
    r._id = vals[offset+2]
    r.gen = vals[offset+3]
    r.n_children = 0
    p = Promise.__new__(Promise)
    p.r = r
    return p

def pack_promise(p):
    p.r.n_children += 1
    return p.r.owner, p.r.creator, p.r._id, p.r.gen + 1

promise_fmt = 'llll'

def pr_v_fmt(n_v_bytes):
    return promise_fmt + str(n_v_bytes) + 's'

def set_result_encoder(pr, v):
    is_bytes = type(v) is bytes
    if is_bytes:
        v_b = v
    else:
        v_b = taskloaf.serialize.dumps(v)
    n_v_bytes = len(v_b)
    sr_fmt = '?l' + pr_v_fmt(n_v_bytes)
    return struct_pack(sr_fmt, is_bytes, n_v_bytes, *pack_promise(pr), v_b)

def set_result_decoder(w, b):
    is_bytes, n_v_bytes = struct.unpack_from('?l', b)
    vals = struct.unpack_from(pr_v_fmt(n_v_bytes), b, offset = struct.calcsize('?l'))
    out_v = vals[4]
    if not is_bytes:
        out_v = taskloaf.serialize.loads(w, out_v)
    return unpack_promise(w, vals, 0), out_v

def task_encoder(f, pr):
    if type(f) is bytes:
        f_b = f
    else:
        f_b = taskloaf.serialize.dumps(f)
    n_f_bytes = len(f_b)
    tsk_fmt = 'l' + pr_v_fmt(n_f_bytes)
    return struct_pack(tsk_fmt, n_f_bytes, *pack_promise(pr), f_b)

def task_decoder(w, b):
    n_f_bytes = struct.unpack_from('l', b)[0]
    vals = struct.unpack_from(pr_v_fmt(n_f_bytes), b, offset = 8)
    return vals[4], unpack_promise(w, vals, 0)



await_fmt = promise_fmt + 'l'
def await_encoder(pr, req_addr):
    return struct_pack(await_fmt, *pack_promise(pr), req_addr)

def await_decoder(w, b):
    vals = struct.unpack_from(await_fmt, b)
    return unpack_promise(w, vals, 0), vals[4]

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
    f = data[0]
    out_pr = data[1]
    args = data[2:]
    async def task_runner(w):
        assert(w.comm.addr == out_pr.owner)
        if isinstance(f, bytes):
            f_b = taskloaf.serialize.loads(w, f)
        else:
            f_b = f
        _unwrap_promise(out_pr, await w.wait_for_work(f_b, *args))
    return task_runner

def task(w, f, *args, to = None, out_pr = None):
    if out_pr is None:
        if to is None:
            to = w.comm.addr
        out_pr = Promise(w, to)

    msg = [f, out_pr] + list(args)
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

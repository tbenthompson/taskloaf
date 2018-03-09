import asyncio
import capnp

import taskloaf.serialize
import taskloaf.refcounting
from taskloaf.ref import put, is_ref

class TaskMsg:
    @staticmethod
    def serialize(args):
        pr = args[0]
        objrefs = args[1:]

        m = taskloaf.message_capnp.Message.new_message()
        m.init('task')

        pr.encode_capnp(m.task.promise)

        m.task.init('objrefs', len(objrefs))
        for i, ref in enumerate(objrefs):
            ref.encode_capnp(m.task.objrefs[i])
        return m

    @staticmethod
    def deserialize(worker, msg):
        out = [
            Promise.decode_capnp(worker, msg.task.promise)
        ]
        for i in range(len(msg.task.objrefs)):
            out.append(taskloaf.ref.ObjectRef.decode_capnp(
                worker, msg.task.objrefs[i]
            ))
        return out

def setup_plugin(worker):
    worker.promises = dict()
    worker.protocol.add_msg_type(
        'TASK',
        type = TaskMsg,
        handler = task_handler
    )
    worker.protocol.add_msg_type(
        'SETRESULT',
        type = TaskMsg,
        handler = set_result_handler
    )
    worker.protocol.add_msg_type(
        'AWAIT',
        type = TaskMsg,
        handler = await_handler
    )

def await_handler(worker, args):
    pr = args[0]
    req_addr = worker.cur_msg.sourceAddr
    async def await_wrapper(worker):
        result_ref = await pr._get_future()
        worker.send(req_addr, worker.protocol.SETRESULT, [pr, result_ref])
    worker.run_work(await_wrapper)

class Promise:
    def __init__(self, worker, running_on):
        def on_delete(_id):
            del worker.promises[_id]
        self.ref = taskloaf.refcounting.Ref(worker, on_delete)
        self.running_on = running_on
        self.ensure_future_exists()

    @property
    def worker(self):
        return self.ref.worker

    def encode_capnp(self, msg):
        self.ref.encode_capnp(msg.ref)
        msg.runningOn = self.running_on

    @classmethod
    def decode_capnp(cls, worker, msg):
        out = Promise.__new__(Promise)
        out.ref = taskloaf.refcounting.Ref.decode_capnp(worker, msg.ref)
        out.running_on = msg.runningOn
        return out

    def ensure_future_exists(self):
        self.worker.promises[self.ref._id] = asyncio.Future()

    def _get_future(self):
        return self.worker.promises[self.ref._id]

    def __await__(self):
        if self.worker.addr != self.ref.owner:
            self.ensure_future_exists()
            self.worker.send(self.ref.owner, self.worker.protocol.AWAIT, [self])
        result_ref = yield from self._get_future().__await__()
        out = yield from result_ref.get().__await__()
        return out

        # else:
        #     self.worker.promises[self.ref._id]
        #     self.worker.send(self.
        # future = yield from self.await_ref.get().__await__()
        # result_ref = yield from future
        # out = yield from result_ref.get().__await__()
        # return out
        # res_dref = yield from self.await_result_dref()
        # out = yield from remote_get(self.worker, res_dref).__await__()
        # return out

    # def await_result_dref(self):
        # here = self.worker.comm.addr
        # if here == self.owner:
        #     self. ref =
        # self.ensure_future_exists()
        # pr_data = self.worker.memory.get_local(self.dref)
        # if here != self.owner and not pr_data[1]:
        #     pr_data[1] = True
        #     self.worker.send(self.owner, self.worker.protocol.AWAIT, [self.dref])
        # return pr_data[0]

    def set_result(self, result):
        self._get_future().set_result(result)
        # self.ensure_future_exists()
        # res_dref = self.worker.memory.put(value = result)
        # self.worker.memory.get_local(self.dref)[0].set_result(res_dref)

    # def then(self, f, to = None):
    #     if to is None:
    #         to = self.owner
    #     out_pr = Promise(DistributedRef(self.worker, to))

    #     async def wait_to_start(worker):
    #         v = await self
    #         task(worker, f, v, out_pr = out_pr)
    #     self.worker.submit_work(self.owner, wait_to_start)
    #     return out_pr

    # def next(self, f, to = None):
    #     return self.then(lambda x: f(), to)


def task_runner(worker, pr, in_f, *in_args):
    async def task_wrapper(worker):
        f = await ensure_obj(in_f)
        args = []
        for a in in_args:
            args.append(await ensure_obj(a))
        try:
            result = await worker.wait_for_work(f, *args)
        # catches all exceptions except system-exiting exceptions that inherit
        # from BaseException
        except Exception as e:
            worker.log.exception('exception during task')
            result = e
        _unwrap_promise(worker, pr, result)
    worker.run_work(task_wrapper)

def set_result(pr, result_ref):
    pr.set_result(result_ref)

def _unwrap_promise(worker, pr, result):
    result_ref = put(worker, result)
    if isinstance(result, Promise):
        pass
        # result.then(lambda w, x: pr.set_result(ref))
    else:
        if pr.ref.owner == worker.addr:
            set_result(pr, result_ref)
        else:
            worker.send(pr.ref.owner, worker.protocol.SETRESULT, [pr, result_ref])

def task_handler(worker, args):
    task_runner(worker, args[0], args[1], *args[2:])

def set_result_handler(worker, args):
    set_result(args[0], args[1])

# f and args can be provided in two forms:
# -- a python object (f should be callable or awaitable)
# -- a dref to a serialized object in the memory manager
# if f is a function and the task is being run locally, f is never serialized, but when the task is being run remotely, f is entered into the
def task(worker, f, *args, to = None):
    if to is None:
        to = worker.addr
    out_pr = Promise(worker, to)
    if to == worker.addr:
        task_runner(worker, out_pr, f, *args)
    else:
        msg_objs = [
            out_pr,
            ensure_ref(worker, f)
        ] + [ensure_ref(worker, a) for a in args]
        worker.send(to, worker.protocol.TASK, msg_objs)
    return out_pr

async def ensure_obj(maybe_ref):
    if is_ref(maybe_ref):
        return await maybe_ref.get()
    else:
        return maybe_ref

def ensure_ref(worker, v):
    if is_ref(v):
        return v
    return put(worker, v)

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

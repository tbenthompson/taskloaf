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
    # worker.protocol.add_msg_type(
    #     'SETRESULT',
    #     serializer = DRefListSerializer,
    #     handler = set_result_builder
    # )
    # worker.protocol.add_msg_type(
    #     'AWAIT',
    #     serializer = DRefListSerializer,
    #     handler = await_builder
    # )

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
        # fut_val = await remote_get(worker, args[1])
        # worker.memory.put(value = fut_val, dref = args[1])
        worker.memory.get_local(args[0])[0].set_result(args[1])
    return run

class Promise:
    def __init__(self, worker, running_on):
        def on_delete(_id):
            del worker.promises[_id]
        self.ref = taskloaf.refcounting.Ref(worker, on_delete)
        self.running_on = running_on
        worker.promises[self.ref._id] = asyncio.Future()

    def encode_capnp(self, msg):
        self.ref.encode_capnp(msg.ref)
        msg.runningOn = self.running_on

    @classmethod
    def decode_capnp(cls, worker, msg):
        out = Promise.__new__(Promise)
        out.ref = taskloaf.refcounting.Ref.decode_capnp(worker, msg.ref)
        out.running_on = msg.runningOn
        return out

    # def ensure_future_exists(self):
    #     if not self.worker.memory.available(self.dref):
    #         here = self.worker.comm.addr
    #         self.worker.memory.put(
    #             value = [asyncio.Future(), False],
    #             dref = self.dref
    #         )
    #         if self.owner != here:
    #             async def delete_after_triggered(worker):
    #                 await worker.memory.get_local(self.dref)[0]
    #                 worker.memory.delete(self.dref)
    #             self.worker.run_work(delete_after_triggered)

    def _get_future(self):
        return self.ref.worker.promises[self.ref._id]

    def __await__(self):
        if self.ref.worker.addr == self.running_on:
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


# def task_runner_builder(worker, data):
#     async def task_runner(worker):
#         out_pr = Promise(data[0])
#         assert(worker.comm.addr == out_pr.dref.owner)
#
#         f = data[1]
#         if is_dref(f):
#             f = await remote_get(worker, f)
#
#         if len(data) > 2:
#             args = data[2]
#             if is_dref(args):
#                 args = await remote_get(worker, args)
#             waiter = worker.wait_for_work(f, args)
#         else:
#             waiter = worker.wait_for_work(f)
#         _unwrap_promise(out_pr, await waiter)
#     return task_runner

async def ensure_obj(maybe_ref):
    if is_ref(maybe_ref):
        return await maybe_ref.get()
    else:
        return maybe_ref

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
            result = e
        _unwrap_promise(worker, pr, result)
    worker.run_work(task_wrapper)

def task_handler(worker, args):
    task_runner(worker, args[0], args[1], *args[2:])

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

def _unwrap_promise(worker, pr, result):
    ref = put(worker, result)
    if isinstance(result, Promise):
        result.then(lambda w, x: pr.set_result(ref))
    else:
        pr.set_result(ref)

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

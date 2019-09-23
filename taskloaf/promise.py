import asyncio

import taskloaf
from .refcounting import Ref
from .object_ref import put, is_ref, ObjectRef

import logging

logger = logging.getLogger(__name__)


def await_handler(args):
    req_addr = args[0]
    pr = args[1]

    async def await_wrapper():
        result_ref = await pr._get_future()
        taskloaf.ctx().messenger.send(
            req_addr, taskloaf.ctx().protocol.SETRESULT, [pr, result_ref]
        )

    taskloaf.ctx().executor.run_work(await_wrapper)


class Promise:
    def __init__(self, running_on):
        def on_delete(_id):
            del taskloaf.ctx().promises[_id]

        self.ref = Ref(on_delete)
        self.running_on = running_on
        self.ensure_future_exists()

    def encode_capnp(self, msg):
        self.ref.encode_capnp(msg.ref)
        msg.runningOn = self.running_on

    @classmethod
    def decode_capnp(cls, msg):
        out = Promise.__new__(Promise)
        out.ref = Ref.decode_capnp(msg.ref)
        out.running_on = msg.runningOn
        return out

    def ensure_future_exists(self):
        taskloaf.ctx().promises[self.ref._id] = asyncio.Future(
            loop=taskloaf.ctx().executor.ioloop
        )

    def _get_future(self):
        return taskloaf.ctx().promises[self.ref._id]

    def __await__(self):
        if taskloaf.ctx().name != self.ref.owner:
            self.ensure_future_exists()
            taskloaf.ctx().messenger.send(
                self.ref.owner, taskloaf.ctx().protocol.AWAIT, [self]
            )
        result_ref = yield from self._get_future().__await__()
        out = yield from result_ref.get().__await__()
        if isinstance(out, TaskExceptionCapture):
            raise out.e
        return out

    def set_result(self, result):
        self._get_future().set_result(result)

    def then(self, f, to=None):
        async def wait_to_start():
            v = await self
            return await taskloaf.ctx().executor.wait_for_work(f, v)

        return task(wait_to_start, to=to)

    def next(self, f, to=None):
        return self.then(lambda x: f(), to)


class TaskExceptionCapture:
    def __init__(self, e):
        self.e = e


def task_runner(pr, in_f, *in_args):
    async def task_wrapper():
        f = await ensure_obj(in_f)
        args = []
        for a in in_args:
            args.append(await ensure_obj(a))
        try:
            result = await taskloaf.ctx().executor.wait_for_work(f, *args)
        # catches all exceptions except system-exiting exceptions that inherit
        # from BaseException
        except Exception as e:
            logger.exception("exception during task")
            result = TaskExceptionCapture(e)
        _unwrap_promise(pr, result)

    taskloaf.ctx().executor.run_work(task_wrapper)


def _unwrap_promise(pr, result):
    if isinstance(result, Promise):

        def unwrap_then(x):
            _unwrap_promise(pr, x)

        result.then(unwrap_then)
    else:
        result_ref = put(result)
        if pr.ref.owner == taskloaf.ctx().name:
            pr.set_result(result_ref)
        else:
            taskloaf.ctx().messenger.send(
                pr.ref.owner,
                taskloaf.ctx().protocol.SETRESULT,
                [pr, result_ref],
            )


def task_handler(args):
    task_runner(args[1], args[2], *args[3:])


def set_result_handler(args):
    args[1].set_result(args[2])


# f and args can be provided in two forms:
# -- a python object (f should be callable or awaitable)
# -- a dref to a serialized object in the memory manager
# if f is a function and the task is being run locally, f is never serialized,
# but when the task is being run remotely, f is entered into the
def task(f, *args, to=None):
    ctx = taskloaf.ctx()
    if to is None:
        to = ctx.name
    out_pr = Promise(to)
    if to == ctx.name:
        task_runner(out_pr, f, *args)
    else:
        msg_objs = [out_pr, ensure_ref(f)] + [ensure_ref(a) for a in args]
        ctx.messenger.send(to, ctx.protocol.TASK, msg_objs)
    return out_pr


async def ensure_obj(maybe_ref):
    if is_ref(maybe_ref):
        return await maybe_ref.get()
    else:
        return maybe_ref


def ensure_ref(v):
    if is_ref(v):
        return v
    return put(v)


class TaskMsg:
    @staticmethod
    def serialize(args):
        pr = args[0]
        objrefs = args[1:]

        m = taskloaf.message_capnp.Message.new_message()
        m.init("task")

        pr.encode_capnp(m.task.promise)

        m.task.init("objrefs", len(objrefs))
        for i, ref in enumerate(objrefs):
            ref.encode_capnp(m.task.objrefs[i])
        return m

    @staticmethod
    def deserialize(msg):
        out = [msg.sourceName, Promise.decode_capnp(msg.task.promise)]
        for i in range(len(msg.task.objrefs)):
            out.append(ObjectRef.decode_capnp(msg.task.objrefs[i]))
        return out


def when_all(ps, to=None):
    if to is None:
        to = ps[0].running_on

    async def wait_for_all():
        results = []
        for i, p in enumerate(ps):
            results.append(await p)
        return results

    return task(wait_for_all, to=to)

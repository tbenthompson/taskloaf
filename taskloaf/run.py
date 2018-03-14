import taskloaf.worker
import taskloaf.promise
import taskloaf.allocator
import taskloaf.refcounting
import taskloaf.object_ref
import contextlib

def add_plugins(worker):
    taskloaf.allocator.setup_plugin(worker)
    taskloaf.refcounting.setup_plugin(worker)
    taskloaf.object_ref.setup_plugin(worker)
    taskloaf.promise.setup_plugin(worker)
    return worker

@contextlib.contextmanager
def null_comm_worker():
    cfg = dict()
    try:
        with taskloaf.worker.Worker(taskloaf.worker.NullComm(), cfg) as worker:
            add_plugins(worker)
            yield worker
    finally:
        pass

def run(coro, cfg = None):
    if cfg is None:
        cfg = dict()

    async def wrapper(worker):
        try:
            worker.result = await coro(worker)
        finally:
            taskloaf.worker.shutdown(worker)

    with null_comm_worker() as worker:
        worker.start(wrapper)
        return worker.result

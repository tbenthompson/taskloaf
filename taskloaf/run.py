import taskloaf.worker
import taskloaf.promise
import taskloaf.allocator
import taskloaf.refcounting
import taskloaf.ref
import contextlib

def add_plugins(worker):
    taskloaf.allocator.setup_plugin(worker)
    taskloaf.refcounting.setup_plugin(worker)
    taskloaf.ref.setup_plugin(worker)
    taskloaf.promise.setup_plugin(worker)
    return worker

@contextlib.contextmanager
def null_comm_worker():
    try:
        with taskloaf.worker.Worker(taskloaf.worker.NullComm()) as worker:
            add_plugins(worker)
            yield worker
    finally:
        pass

def run(coro):
    async def wrapper(worker):
        try:
            worker.result = await coro(worker)
        finally:
            taskloaf.worker.shutdown(worker)

    with null_comm_worker() as worker:
        worker.start(wrapper)
        return worker.result

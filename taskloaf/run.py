import taskloaf.worker
import taskloaf.memory
import taskloaf.promise
import contextlib

def add_plugins(worker):
    store = taskloaf.memory.SerializedMemoryStore(worker.addr, worker.exit_stack)
    worker.memory = taskloaf.memory.MemoryManager(worker, store)
    taskloaf.promise.setup_protocol(worker)
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
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result

    with null_comm_worker() as worker:
        return worker.start(wrapper)

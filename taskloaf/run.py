import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

def add_plugins(w):
    w.memory = taskloaf.memory.MemoryManager(w)
    taskloaf.promise.setup_protocol(w)
    return w

def null_comm_worker():
    return add_plugins(taskloaf.worker.Worker(taskloaf.worker.NullComm()))

def run(coro):
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result

    return null_comm_worker().start(wrapper)

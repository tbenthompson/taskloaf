import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

def add_plugins(w):
    w.memory = taskloaf.memory.MemoryManager(w)
    taskloaf.promise.setup_protocol(w)

def default_worker(comm):
    w = taskloaf.worker.Worker(comm)
    add_plugins(w)
    return w

def run(coro):
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result

    return default_worker(taskloaf.worker.NullComm()).start(wrapper)

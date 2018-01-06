import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

def null_comm_worker():
    worker = taskloaf.worker.Worker(taskloaf.worker.NullComm())
    worker.memory = taskloaf.memory.MemoryManager(worker)
    taskloaf.promise.setup_protocol(worker)
    return worker

def run(coro):
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result

    worker = null_comm_worker()
    return worker.start(wrapper)

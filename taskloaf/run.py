import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

class NullComm:
    def __init__(self):
        self.addr = 0

    def recv(self):
        return None

def run(coro):
    async def wrapper(w):
        w.memory = taskloaf.memory.MemoryManager(w)
        w.promise_manager = taskloaf.promise.PromiseManager(w)
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result
    w = taskloaf.worker.Worker()
    return w.start(NullComm(), [wrapper])

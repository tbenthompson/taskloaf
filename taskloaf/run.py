import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

class NullComm:
    def __init__(self):
        self.addr = 0

    def recv(self):
        return None

def run(coro):
    out = [0]
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        out[0] = result
    w = taskloaf.worker.Worker()
    w.memory = taskloaf.memory.MemoryManager(w)
    w.promise_manager = taskloaf.promise.PromiseManager(w)
    w.start(NullComm(), [wrapper])
    return out[0]

import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

def default_worker():
    w = taskloaf.worker.Worker()
    w.memory = taskloaf.memory.MemoryManager(w)
    taskloaf.promise.setup_protocol(w)
    return w

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

    default_worker().start(NullComm(), [wrapper])
    return out[0]

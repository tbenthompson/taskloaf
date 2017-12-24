import taskloaf.worker
import taskloaf.memory
import taskloaf.promise

def add_plugins(w):
    w.memory = taskloaf.memory.MemoryManager(w)
    taskloaf.promise.setup_protocol(w)

def default_worker():
    w = taskloaf.worker.Worker()
    add_plugins(w)
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

import taskloaf.worker
import taskloaf.memory

class NullComm:
    def __init__(self):
        self.addr = 0

    async def comm_poll(self, tasks):
        pass

def run(coro):
    async def wrapper(w):
        w.memory = taskloaf.memory.MemoryManager(w)
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result
    w = taskloaf.worker.Worker()
    return w.start(NullComm(), [wrapper])

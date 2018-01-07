import numpy as np
import taskloaf as tsk

async def submit(w):
    n = int(1e7)
    A = np.random.rand(n)
    async with tsk.Profiler(w, range(2)):
        dref = w.memory.put(value = A.data)
        async def remote(w):
            return np.sum(np.frombuffer(await tsk.remote_get(w, dref)))
        lhs = await tsk.task(w, remote, to = 1)
        rhs = np.sum(np.frombuffer(w.memory.get(dref)))
        assert(lhs == rhs)

if __name__ == "__main__":
    tsk.cluster(2, submit)

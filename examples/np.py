import numpy as np
import taskloaf as tsk

async def submit(w):
    n = int(4e8)
    ref = tsk.alloc(w, n * 8)
    A = np.frombuffer(await ref.get(), dtype = np.float64)
    A[:] = np.random.rand(n)
    rhs = np.sum(A)
    for to in range(2):
        async with tsk.Profiler(w, range(2)):
            # ref = tsk.put(w, A.data.cast('B'))
            async def remote(w):
                A = np.frombuffer(await ref.get(), dtype = np.float64)
                return np.sum(A)
            lhs = await tsk.task(w, remote, to = 1)
            assert(lhs == rhs)

if __name__ == "__main__":
    tsk.cluster(2, submit)

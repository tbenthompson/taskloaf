import time
import taskloaf as tsk

async def submit(w):
    import numpy as np
    n = int(3e7)
    for i in range(5):
        r = w.memory.put(np.random.rand(n))
        start = time.time()
        async def remote(w):
            return np.sum(await tsk.remote_get(r))
        assert(await tsk.task(w, remote, to = 1) == np.sum(r.get()))
        print(time.time() - start)

if __name__ == "__main__":
    tsk.cluster(2, submit, runner = tsk.mpiexisting)

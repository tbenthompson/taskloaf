import time
import taskloaf as tsk

async def submit(w):
    import numpy as np
    n = int(1e6)
    start = time.time()
    for i in range(10):
        r = w.memory.put(np.random.rand(n))
        async def remote(w):
            return np.sum(await tsk.remote_get(r))
        assert(await tsk.task(w, remote, to = 1) == np.sum(r.get()))
    print(time.time() - start)

if __name__ == "__main__":
    tsk.cluster(2, submit, runner = tsk.mpiexisting)

import cloudpickle
import multiprocessing
import asyncio

import taskloaf.worker

class LocalComm:
    def __init__(self, local_queues, addr):
        self.local_queues = local_queues
        self.addr = addr
        assert(0 <= addr < len(self.local_queues))

    def send(self, to_addr, data):
        self.local_queues[to_addr].put(cloudpickle.dumps(data))

    def recv(self):
        if self.local_queues[self.addr].empty():
            return None
        return cloudpickle.loads(self.local_queues[self.addr].get())

    async def comm_poll(self, tasks):
        while taskloaf.worker.running:
            t = self.recv()
            if t is not None:
                tasks.append(t)
            await asyncio.sleep(0)

def localrun(n_workers, f):
    try:
        p = multiprocessing.Pool(n_workers)
        manager = multiprocessing.Manager()
        qs = [manager.Queue() for i in range(n_workers)]
        args = [(cloudpickle.dumps(f), i, qs) for i in range(n_workers)]
        fut = p.starmap_async(localstart, args)
        return fut.get()[0]
    finally:
        p.close()
        p.join()

def localstart(f, i, qs):
    try:
        c = LocalComm(qs, i)
        return cloudpickle.loads(f)(c)
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise e

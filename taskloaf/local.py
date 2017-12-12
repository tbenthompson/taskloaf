import os
import time
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
        # start = time.time()
        D = cloudpickle.dumps(data)
        # T = time.time() - start
        self.local_queues[to_addr].put(D)
        # print('addr: ', self.addr, 'to: ', to_addr, ' MB: ', len(D) / 1e6, T)

    def recv(self):
        if self.local_queues[self.addr].empty():
            return None
        # import time
        # start = time.time()
        out = cloudpickle.loads(self.local_queues[self.addr].get())
        # T = time.time() - start
        # print('recv: ', T)
        return out

    async def comm_poll(self, tasks):
        while taskloaf.worker.running:
            t = self.recv()
            if t is not None:
                # print('addr: ', self.addr, ' adding task: ', len(tasks))
                tasks.append(t)
            await asyncio.sleep(0)

def localrun(n_workers, f, pin = True):
    try:
        p = multiprocessing.Pool(n_workers)
        manager = multiprocessing.Manager()
        qs = [manager.Queue() for i in range(n_workers)]
        args = [(cloudpickle.dumps(f), i, qs, pin) for i in range(n_workers)]
        fut = p.starmap_async(localstart, args)
        return fut.get()[0]
    finally:
        p.close()
        p.join()

def localstart(f, i, qs, pin):
    try:
        if pin:
            os.system("taskset -p -c %d %d" % ((i % os.cpu_count()), os.getpid()))
        c = LocalComm(qs, i)
        return cloudpickle.loads(f)(c)
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise e

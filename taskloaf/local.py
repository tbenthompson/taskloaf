import os
import time
import multiprocessing
import asyncio

class LocalComm:
    def __init__(self, local_queues, addr):
        self.local_queues = local_queues
        self.addr = addr
        assert(0 <= addr < len(self.local_queues))
        self.i = 0

    def send(self, to_addr, data):
        self.local_queues[to_addr].put(bytes(data), block = False)

    def recv(self):
        self.i += 1
        try:
            return self.local_queues[self.addr].get(block = False)
        except:
            return None

def localrun(n_workers, f):
    try:
        qs = [multiprocessing.Queue() for i in range(n_workers + 1)]
        args = [(f, i, qs) for i in range(n_workers)]
        ps = [
            multiprocessing.Process(target = localstart, args = args[i])
            for i in range(n_workers)
        ]
        for p in ps:
            p.start()
        return qs[-1].get()
    finally:
        for p in ps:
            p.join()


def localstart(f, i, qs):
    # I removed this core pinning because it creates some noise and isn't worth
    # it for the low efficiency multiprocessing comm anyway. Use MPI!
    # and in the future, rather than using taskset, use the cross-platform
    # psutil.cpu_affinity([i])
    #
    # if pin:
    #     os.system("taskset -p -c %d %d" % ((i % os.cpu_count()), os.getpid()))
    c = LocalComm(qs, i)
    out = f(c)
    if c.addr == 0:
        qs[-1].put(out)

import os
import time
import multiprocessing
import asyncio

from taskloaf.serialize import loads, dumps

class LocalComm:
    def __init__(self, local_queues, addr):
        self.local_queues = local_queues
        self.addr = addr
        assert(0 <= addr < len(self.local_queues))
        self.i = 0

    def send(self, to_addr, data):
        self.local_queues[to_addr].put(data, block = False)

    def recv(self):
        self.i += 1
        try:
            return self.local_queues[self.addr].get(block = False)
        except:
            return None

def localrun(n_workers, f, pin = True):
    try:
        qs = [multiprocessing.Queue() for i in range(n_workers)]
        args = [(f, i, qs, pin) for i in range(n_workers)]
        ps = [multiprocessing.Process(target = localstart, args = args[i]) for i in range(n_workers)]
        for p in ps:
            p.start()
    finally:
        for p in ps:
            p.join()

def localstart(f, i, qs, pin):
    try:
        if pin:
            os.system("taskset -p -c %d %d" % ((i % os.cpu_count()), os.getpid()))
        c = LocalComm(qs, i)
        out = f(c)
        return out
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise e

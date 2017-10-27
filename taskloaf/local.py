import cloudpickle
import multiprocessing

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

def localrun(n_workers, f):
    try:
        p = multiprocessing.Pool(n_workers + 1)
        manager = multiprocessing.Manager()
        qs = [manager.Queue() for i in range(n_workers + 1)]
        args = [(cloudpickle.dumps(f), i, qs) for i in range(n_workers + 1)]
        fut = p.starmap_async(localstart, args)
        fut.wait()
    finally:
        p.close()
        p.join()

def localstart(f, i, qs):
    try:
        c = LocalComm(qs, i)
        cloudpickle.loads(f)(c)
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise e

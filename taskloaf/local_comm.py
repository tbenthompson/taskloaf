class LocalComm:
    def __init__(self, local_queues, addr):
        self.local_queues = local_queues
        self.addr = addr

    def send(self, to_addr, data):
        self.local_queues[to_addr].put(data)

    def recv(self):
        return self.local_queues[self.addr].get()

from multiprocessing import Process, Queue
def queue_test_helper(q):
    q.put(123)

def test_queue():
    q = Queue()
    p = Process(target = queue_test_helper, args = (q, ))
    p.start()
    assert(q.get() == 123)
    p.join()

def test_local_comm():
    qs = [Queue(), Queue()]
    c0 = LocalComm(qs, 0)
    c1 = LocalComm(qs, 1)
    c0.send(1, 456)
    assert(c1.recv() == 456)

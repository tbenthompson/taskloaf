import cloudpickle

class LocalComm:
    def __init__(self, local_queues, addr):
        self.local_queues = local_queues
        self.addr = addr
        assert(0 <= addr < len(self.local_queues))

    def send(self, to_addr, data):
        self.local_queues[to_addr].put(cloudpickle.dumps(data))

    def recv(self):
        return cloudpickle.loads(self.local_queues[self.addr].get())

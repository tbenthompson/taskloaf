import multiprocessing
import cloudpickle
import psutil
import zmq

class ZMQComm:
    def __init__(self, addr, hosts):
        self.addr = addr
        self.hosts = hosts
        self.ctx = zmq.Context()
        self.recv_socket = self.ctx.socket(zmq.PULL)
        # self.recv_socket.setsockopt(zmq.SUBSCRIBE,b'')

        self.hostname, self.port = self.hosts[self.addr]
        self.recv_socket.bind(self.hostname % self.port)

        self.sync_socket = self.ctx.socket(zmq.REP)
        self.sync_socket.bind(self.hostname % (self.port + 1))

        self.send_sockets = []
        for i in range(len(self.hosts)):
            if i == addr:
                self.send_sockets.append(None)
            else:
                s = self.ctx.socket(zmq.PUSH)
                s.setsockopt(zmq.LINGER, 0)
                other_hostname, other_port = self.hosts[i]
                s.connect(other_hostname % other_port)
                self.send_sockets.append(s)

        self._check_sockets_ready()
        self._sync()

    def _check_sockets_ready(self):
        leader = 0
        if self.addr == 0:
            count = 1
            while count < len(self.hosts):
                while self.recv_socket.poll(timeout = 0) == 0:
                    for i in range(len(self.hosts)):
                        if i == self.addr:
                            continue
                        self.send_sockets[i].send(b'')
                self.recv_socket.recv()
                count += 1
        else:
            self.recv_socket.recv()
            other_hostname, other_port = self.hosts[leader]
            self.send_sockets[leader].send(b'')

    def _sync(self):
        leader = 0
        if self.addr == leader:
            for i in range(len(self.hosts)):
                if i == leader:
                    continue
                other_hostname, other_port = self.hosts[i]
                with self.ctx.socket(zmq.REQ) as syncreq_socket:
                    syncreq_socket.connect(other_hostname % (other_port + 1))
                    syncreq_socket.send(b'')
                    syncreq_socket.recv()
        else:
            self.sync_socket.recv()
            self.sync_socket.send(b'')

    def close(self):
        for s in self.send_sockets:
            if s is not None:
                s.close()
        self.recv_socket.close()
        self.sync_socket.close()
        self.ctx.destroy(linger = 0)

    def send(self, to_addr, data):
        self.send_sockets[to_addr].send_multipart([data])

    def recv(self):
        if self.recv_socket.poll(timeout = 0) == 0:
            return None
        msg = self.recv_socket.recv_multipart()[0]
        if len(msg) == 0:
            return None
        return msg

    def barrier(self):
        self._sync()

def zmqrun(n_workers, f, cfg):
    try:
        q = multiprocessing.Queue()
        ports = cfg.get(
            'zmq_ports',
            [5755 + 2 * i for i in range(n_workers)]
        )
        hostnames = cfg.get(
            'zmq_hostnames',
            ['tcp://127.0.0.1:%s' for i in range(n_workers)]
        )
        hosts = [(h, p) for h, p in zip(hostnames, ports)]
        ps = [
            multiprocessing.Process(
                target = zmqstart,
                args = (cloudpickle.dumps(f), i, hosts, q)
            ) for i in range(n_workers)
        ]
        for p in ps:
            p.start()
        return q.get()
    finally:
        for p in ps:
            p.join()

def zmqstart(f, i, hosts, q):
    n_cpus = psutil.cpu_count()
    psutil.Process().cpu_affinity([i % n_cpus])
    c = ZMQComm(i, hosts)
    out = cloudpickle.loads(f)(c)
    c.close()
    if i == 0:
        q.put(out)

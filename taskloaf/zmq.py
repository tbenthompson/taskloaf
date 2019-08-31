import multiprocessing
from contextlib import ExitStack, closing, contextmanager

import cloudpickle
import psutil
import zmq

@contextmanager
def zmq_context():
    ctx = zmq.Context()
    yield ctx
    ctx.destroy(linger = 0)

class Friend:
    def __init__(self, hostname, port):
        self.name = hash((hostname, port))
        self.hostname = hostname
        self.port = port

class ZMQComm:
    def __init__(self, port, initial_friends, hostname = 'tcp://*'):
        self.port = port
        self.hostname = hostname
        self.initial_friends = initial_friends
        self.friends = dict()

    def __enter__(self):
        self.exit_stack = ExitStack()

        self.ctx = self.exit_stack.enter_context(zmq_context())
        self.recv_socket = self.exit_stack.enter_context(closing(
            self.ctx.socket(zmq.PULL)
        ))
        # self.recv_socket.setsockopt(zmq.SUBSCRIBE,b'')
        self.recv_socket.bind(self.hostname + ':' + str(self.port))

        for f in self.initial_friends:
            self.add_friend(f[0], f[1])

        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def add_friend(self, hostname, port):
        f = Friend(hostname, port)
        f.send_socket = self.exit_stack.enter_context(closing(
            self.ctx.socket(zmq.PUSH)
        ))
        f.send_socket.setsockopt(zmq.LINGER, 0)
        f.send_socket.connect(f.hostname + ':' + str(f.port))
        self.friends[f.name] = f

    def send(self, to_addr, data):
        print(to_addr, data)
        self.friends[to_addr].send_socket.send_multipart([data])

    def recv(self):
        if self.recv_socket.poll(timeout = 0) == 0:
            return None
        msg = self.recv_socket.recv_multipart()[0]
        if len(msg) == 0:
            return None
        return msg

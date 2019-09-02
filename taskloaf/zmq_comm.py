from contextlib import ExitStack, closing, contextmanager

import zmq
import zmq.asyncio


@contextmanager
def zmq_context():
    ctx = zmq.asyncio.Context()
    yield ctx
    ctx.destroy(linger=0)


class ZMQComm:
    """
    What is hidden inside ZMQComm?...
    """

    def __init__(self, addr):
        self.addr = addr
        self.hostname = addr[0]
        self.port = addr[1]

    def __enter__(self):
        self.exit_stack = ExitStack()

        self.ctx = self.exit_stack.enter_context(zmq_context())
        self.recv_socket = self.exit_stack.enter_context(
            closing(self.ctx.socket(zmq.PULL))
        )
        # self.recv_socket.setsockopt(zmq.SUBSCRIBE,b'')
        self.recv_socket.bind(self.hostname + ":" + str(self.port))

        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def connect(self, addr):
        send_socket = self.exit_stack.enter_context(
            closing(self.ctx.socket(zmq.PUSH))
        )
        send_socket.setsockopt(zmq.LINGER, 0)
        send_socket.connect(addr[0] + ":" + str(addr[1]))
        return send_socket

    def disconnect(self, socket):
        pass
        # TODO:???
        # socket.close()

    def send(self, socket, data):
        # TODO: allow multipart messages
        socket.send_multipart([data])

    async def recv(self):
        full_msg = await self.recv_socket.recv_multipart()
        msg = full_msg[0]
        if len(msg) == 0:
            return None
        return msg

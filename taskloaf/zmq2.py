import time
import zmq
import pickle
import attr

@attr.s
class Remote:
    addr = attr.ib()
    id_ = attr.ib()
    send_socket = attr.ib()
    initialized = attr.ib(default = False)

def get_remote_id(addr):
    #TODO: hash to get id/addr
    # currently, just getting the port
    return int(addr[-4:])


# This has two states:
# 1) Connecting to first end point.
# 2) Waiting for recv_socket to be ready.
# 3) Receiving and responding to messages:
#    -- if an empty message is received, reply with a one byte message
#    -- if a request is received on the sync_socket, reply with the list of end
#    points you currently know about (TODO: Can we get rid of the sync_socket?
#    synchronization is going the way of the mammoth.)
#    -- if any other request is received, return it

# What about instead, a one state solution. Set up the recv socket and any join remote
class ZMQComm:
    def __init__(self, cfg):
        # self.addr = addr
        # self.hosts = hosts
        self.remotes = dict()
        self.ctx = zmq.Context()

        self.addr = cfg['zmq_addr']

        self.remotes = dict()
        self.id_ = self._setup_remote(self.addr)
        self.remotes[self.id_].initialized = True

        #TODO: Try SUB? Upsides, downsides?
        self.recv_socket = self.ctx.socket(zmq.PULL)
        self.recv_socket.bind(self.addr)

        #TODO: Use standard nomenclature for hostnames, ports, hosts, addresses?
        if cfg['zmq_join_addr'] is not None:
            self._setup_remote(cfg['zmq_join_addr'])

    def _setup_remote(self, addr):
        id_ = get_remote_id(addr)
        if id_ not in self.remotes:
            send_socket = self.ctx.socket(zmq.PUSH)
            send_socket.connect(addr)
            send_socket.setsockopt(zmq.LINGER, 0)
            self.remotes[id_] = Remote(
                addr = addr,
                id_ = id_,
                send_socket = send_socket,
            )
        return id_

    def _meet(self, _id):
        self.remotes[_id].send_socket.send_multipart([
            bytes([0]),
            pickle.dumps(
                [self.addr] +
                [v.addr for k, v in self.remotes.items()]
            )
        ])

    def _new_remotes(self, remotes):
        #TODO: Do a set diff of the sent remotes and the current ones here and decide whether to reply.
        for addr in remotes:
            id_ = self._setup_remote(addr)
            if id_ == self.id_:
                continue

        sender_addr = remotes[0]
        sender_id = get_remote_id(sender_addr)
        print(self.id_, 'recv from', sender_id, remotes)
        # means can recv, but should still send
        self.remotes[sender_id].initialized = True

    def _notify_unitiailized_remotes(self):
        for k, r in self.remotes.items():
            if r.initialized:
                continue
            self._meet(k)

    def send(self, to_addr, data):
        self.remotes[to_addr].send_socket.send_multipart([bytes([1]), data])

    def recv(self):
        self._notify_unitiailized_remotes()
        if self.recv_socket.poll(timeout = 0) == 0:
            return None
        msg = self.recv_socket.recv_multipart()
        if msg[0][0] == 0:
            self._new_remotes(pickle.loads(msg[1]))
            return None
        return msg[1]


#
#     def _wait_for_a_connection(self):
#         print('wait for connection')
#         if len(self.remotes) == 0:
#             #TODO: timeout infinite? wait forever until someone connects.
#             #TODO: log that we are waiting for a connection
#             self.sync_socket.recv()
#             self.sync_socket.send(cloudpickle.dumps(self.remotes))
#
#     def _check_recv_ready(self):
#         print('check recv ready')
#         while self.recv_socket.poll(timeout = 0) == 0:
#             self.remotes[0].send(b'')
#         self._handle_msg(self.recv_socket.recv())

        # self.send_sockets = []
        # for i in range(len(self.hosts)):
        #     if i == addr:
        #         self.send_sockets.append(None)
        #     else:
        #         s = self.ctx.socket(zmq.PUSH)
        #         s.setsockopt(zmq.LINGER, 0)
        #         other_hostname, other_port = self.hosts[i]
        #         s.connect(other_hostname % other_port)
        #         self.send_sockets.append(s)

        # self._check_sockets_ready()
        # self._sync()

    # def _check_sockets_ready(self):
    #     leader = 0
    #     if self.addr == 0:
    #         count = 1
    #         while count < len(self.hosts):
    #             while self.recv_socket.poll(timeout = 0) == 0:
    #                 for i in range(len(self.hosts)):
    #                     if i == self.addr:
    #                         continue
    #                     self.send_sockets[i].send(b'')
    #             self.recv_socket.recv()
    #             count += 1
    #     else:
    #         self.recv_socket.recv()
    #         other_hostname, other_port = self.hosts[leader]
    #         self.send_sockets[leader].send(b'')

    # def _sync(self):
    #     leader = 0
    #     if self.addr == leader:
    #         for i in range(len(self.hosts)):
    #             if i == leader:
    #                 continue
    #             other_hostname, other_port = self.hosts[i]
    #             with self.ctx.socket(zmq.REQ) as syncreq_socket:
    #                 syncreq_socket.connect(other_hostname % (other_port + 1))
    #                 syncreq_socket.send(b'')
    #                 syncreq_socket.recv()
    #     else:
    #         self.sync_socket.recv()
    #         self.sync_socket.send(b'')

    # def close(self):
    #     for s in self.send_sockets:
    #         if s is not None:
    #             s.close()
    #     self.recv_socket.close()
    #     self.sync_socket.close()
    #     self.ctx.destroy(linger = 0)

    # def send(self, to_addr, data):
    #     self.send_sockets[to_addr].send_multipart([data])

    # def recv(self):
    #     if self.recv_socket.poll(timeout = 0) == 0:
    #         return None
    #     msg = self.recv_socket.recv_multipart()[0]
    #     if len(msg) == 0:
    #         return None
    #     return msg

    # def barrier(self):
    #     self._sync()


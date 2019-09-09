from .protocol import Protocol

import logging

log = logging.getLogger(__name__)


class JoinMeetMessenger:
    """
    A cluster view is a pairing of a communicator, ownership of the protocol,
    and the glue to attach the two. That includes control over finding other
    messengers over the network.

        MEET = send a message asking to JOIN
               receiver sends a JOIN reply

        JOIN = send a message with tuples of form (name, hostname, port) for
               all friends receiver then JOIN all new friends (possibly
               including the one they received the JOIN from)

        A worker will MEET then receive the JOIN replies and JOIN all the new
        friends.

        A client will MEET then receive the JOIN replies and MEET all the new
        friends. As a result, a client learns about the whole cluster but the
        cluster doesn't know about the client or send the client info. Is
        this a good idea? Then how to send info out to the client? By
        hostname/port?
    """

    def __init__(self, name, comm, should_join):
        self.name = name
        self.comm = comm
        self.should_join = should_join

        self.endpts = dict()

        self.protocol = Protocol()

        self.setup_protocol()

    def setup_protocol(self):
        def handle_meet(args):
            name, addr = args

            self.connect(name, addr)
            self.join(name, addr)

        # TODO optimize with capnp
        self.protocol.add_msg_type("MEET", handler=handle_meet)

        def handle_join(new_endpts):
            is_new = [self.connect(name, addr) for (name, addr) in new_endpts]

            if not self.should_join:
                return

            for i, (name, addr) in enumerate(new_endpts):
                if i == 0:
                    skip = [v[0] for v in new_endpts]
                else:
                    skip = []
                if is_new[i]:
                    self.join(name, addr, skip)

        # TODO optimize with capnp
        self.protocol.add_msg_type("JOIN", handler=handle_join)

    def join(self, name, addr, known_endpts=None):
        if known_endpts is None:
            known_endpts = []

        endpt_list = []
        if self.name not in known_endpts:
            endpt_list.append((self.name, self.comm.addr))
        for endpt_name, (_, endpt_addr) in self.endpts.items():
            if endpt_name in known_endpts:
                continue
            endpt_list.append((endpt_name, endpt_addr))

        if len(endpt_list) == 0:
            return
        self.send(name, self.protocol.JOIN, endpt_list)

    def connect(self, name, addr):
        if name in self.endpts or name == self.name:
            return False
        endpt = self.comm.connect(addr)
        self.endpts[name] = (endpt, addr)
        return True

    def send(self, to_name, msg_type_code, msg_args, connect_addr=None):
        log.info(
            f"send {self.protocol.get_name(msg_type_code)}"
            f" to {to_name} with data: {str(msg_args)}"
        )
        data = self.protocol.encode(self.name, msg_type_code, msg_args)
        if to_name not in self.endpts and connect_addr is not None:
            self.connect(to_name, connect_addr)
        self.comm.send(self.endpts[to_name][0], data)

    async def recv(self):
        msg_buf = await self.comm.recv()
        if msg_buf is not None:
            # TODO: should memoryview call be here? or inside protocol?
            self.protocol.handle(memoryview(msg_buf))
            return True
        else:
            return False

    def meet(self, addr):
        log.info(f"meet {addr}")
        if addr == self.comm.addr:
            return
        data = self.protocol.encode(
            self.name, self.protocol.MEET, (self.name, self.comm.addr)
        )
        endpt = self.comm.connect(addr)
        self.comm.send(endpt, data)
        self.comm.disconnect(endpt)

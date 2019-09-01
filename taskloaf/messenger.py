
from .protocol import Protocol

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
    def __init__(self, name, comm):
        self.name = name
        self.comm = comm

        self.endpts = dict()

        self.protocol = Protocol()
        self.setup_protocol()

    def setup_protocol(self):
        def handle_meet(args):
            print(f'meet on {self.name}', args)
            their_name, their_addr = args
            self.join(their_name, their_addr)


        #TODO optimize with capnp
        self.protocol.add_msg_type('MEET', handler = handle_meet)

        def handle_join(args):
            print(f'join on {self.name}', args)
            new_endpts = args
            sender_name, sender_addr = new_endpts[0]
            self.join(sender_name, sender_addr)
            if not self.join(sender_name, sender_addr):
                send_endpt_names = set(self.endpts.keys()) - set([v[0] for v in new_endpts])
                print('sender missing', send_endpt_names)
                if len(send_endpt_names) > 0:
                    send_endpts = []
                    for name in send_endpt_names:
                        send_endpts.append((name, self.endpts[name][1]))
                    self.send(sender_name, self.protocol.JOIN, send_endpts)

            for their_name, their_addr in new_endpts[1:]:
                self.join(their_name, their_addr)

            # self.comm.add_friend(

        #TODO optimize with capnp
        self.protocol.add_msg_type('JOIN', handler = handle_join)

    def join(self, their_name, their_addr):
        if their_name in self.endpts or their_name == self.name:
            return False
        self.connect(their_name, their_addr)

        endpt_list = [(self.name, self.comm.addr)]
        for endpt_name, (_, endpt_addr) in self.endpts.items():
            if endpt_name == their_name:
                continue
            endpt_list.append((endpt_name, endpt_addr))
        self.send(their_name, self.protocol.JOIN, endpt_list)
        return True

    def connect(self, their_name, their_addr):
        their_endpt = self.comm.connect(their_addr)
        self.endpts[their_name] = (their_endpt, their_addr)

    def send(self, to_name, msg_type_code, *msg_args):
        print(f'sending from {self.name} to', to_name,
                self.protocol.get_name(msg_type_code), *msg_args)
        data = self.protocol.encode(self.name, msg_type_code, *msg_args)
        self.comm.send(self.endpts[to_name][0], data)

    def recv(self):
        msg_buf = self.comm.recv()
        # print(msg)
        if msg_buf is not None:
            #TODO: should memoryview call be here? or inside protocol?
            self.protocol.handle(memoryview(msg_buf))

    def meet(self, addr):
        data = self.protocol.encode(
            self.name,
            self.protocol.MEET,
            (self.name, self.comm.addr)
        )
        #TODO: leaking a socket here.
        endpt = self.comm.connect(addr)
        self.comm.send(endpt, data)
        self.comm.disconnect(endpt)

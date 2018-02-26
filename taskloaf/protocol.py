import capnp
import taskloaf.message_capnp
import taskloaf.serialize

class CloudPickleMsg:
    @staticmethod
    def serialize(args):
        m = taskloaf.message_capnp.Message.new_message()
        m.init('arbitrary')
        m.arbitrary.bytes = taskloaf.serialize.dumps(args)
        return m

    @staticmethod
    def deserialize(w, m):
        return taskloaf.serialize.loads(w, m.arbitrary.bytes)

class Protocol:
    def __init__(self):
        self.msg_types = []

    def add_msg_type(self, name, *,
            type = CloudPickleMsg,
            handler = None):

        type_code = len(self.msg_types)
        setattr(self, name, type_code)
        self.msg_types.append((type, handler, name))
        return type_code

    def encode(self, worker, type_code, *args):
        serializer = self.msg_types[type_code][0].serialize
        m = serializer(*args)
        m.typeCode = type_code
        m.sourceAddr = worker.addr
        return memoryview(m.to_bytes())

    def decode(self, worker, b):
        m = taskloaf.message_capnp.Message.from_bytes(b)
        deserialize = self.msg_types[m.typeCode][0].deserialize
        return m, deserialize(worker, m)

    def handle(self, worker, type_code, x):
        return self.msg_types[type_code][1](worker, x)

    def get_name(self, type_code):
        return self.msg_types[type_code][2]

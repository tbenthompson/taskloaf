import capnp
import taskloaf.message_capnp
import taskloaf.memory
import taskloaf.serialize

class CloudPickleSerializer:
    @staticmethod
    def serialize(type_code, args):
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('arbitrary')
        m.arbitrary.bytes = taskloaf.serialize.dumps(args)
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return taskloaf.serialize.loads(w, m.arbitrary.bytes)

class Protocol:
    def __init__(self):
        self.msg_types = []

    def add_msg_type(self, name, *,
            serializer = CloudPickleSerializer,
            handler = None):

        type_code = len(self.msg_types)
        setattr(self, name, type_code)
        self.msg_types.append((serializer, handler, name))
        return type_code

    def encode(self, type_code, *args):
        serialize = self.msg_types[type_code][0].serialize
        return memoryview(serialize(type_code, *args))

    def decode(self, worker, b):
        m = taskloaf.message_capnp.Message.from_bytes(b)
        deserialize = self.msg_types[m.typeCode][0].deserialize
        return m.typeCode, deserialize(worker, m)

    def handle(self, type_code, x):
        return self.msg_types[type_code][1](x)

    def get_name(self, type_code):
        return self.msg_types[type_code][2]

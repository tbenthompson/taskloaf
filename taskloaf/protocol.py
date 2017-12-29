import capnp
import taskloaf.message_capnp
import taskloaf.memory
import taskloaf.serialize

def default_work_builder(x):
    return x[0]

class CloudPickleSerializer:
    @staticmethod
    def serialize(type_code, drefs):
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('arbitrary')
        m.arbitrary.bytes = taskloaf.serialize.dumps(args)
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return taskloaf.serialize.loads(w, m.arbitrary.bytes)

class DRefListSerializer:
    @staticmethod
    def serialize(type_code, drefs):
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('drefs', len(drefs))
        for i in range(len(drefs)):
            drefs[i].encode_capnp(m.drefs[i])
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return [
            taskloaf.memory.DistributedRef.decode_capnp(w, dr)
            for dr in m.drefs
        ]

class Protocol:
    def __init__(self):
        self.handlers = []

    def add_handler(self, name, *,
            encoder = default_encoder,
            decoder = default_decoder,
            work_builder = default_work_builder):

        type_code = len(self.handlers)
        setattr(self, name, type_code)
        self.handlers.append((encoder, decoder, work_builder, name))
        return type_code

    def encode(self, type_code, *args):
        return memoryview(self.handlers[type_code][0](type_code, *args))

    def decode(self, w, b):
        m = taskloaf.message_capnp.Message.from_bytes(b)
        return m.typeCode, self.handlers[m.typeCode][1](w, m)

    def build_work(self, type_code, x):
        return self.handlers[type_code][2](x)

    def get_name(self, type_code):
        return self.handlers[type_code][3]

import pickle

import taskloaf.serialize

type_code_size = 12

# TODO: Better (more standard?) naming here.
class Protocol:
    def __init__(self):
        self.handlers = []

    def add_handler(self, name, *, encoder = None, decoder = None, work_builder = None):

        if encoder is None:
            encoder = lambda x: [taskloaf.serialize.dumps(x)]

        if decoder is None:
            decoder = lambda w, b: taskloaf.serialize.loads(w, b[0])

        if work_builder is None:
            work_builder = lambda x: x

        self.handlers.append((encoder, decoder, work_builder, name))
        return len(self.handlers) - 1

    def encode(self, type, obj):
        b = self.handlers[type][0](obj)
        return [pickle.dumps(type)] + b

    def decode(self, w, bytes):
        type = pickle.loads(bytes[0])
        return type, self.handlers[type][1](w, bytes[1:])

    def build_work(self, type, x):
        return self.handlers[type][2](x)

    def get_name(self, type):
        return self.handlers[type][3]

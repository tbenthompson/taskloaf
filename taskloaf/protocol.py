import pickle

import taskloaf.serialize

type_code_size = 12

# TODO: Better (more standard?) naming here.
class Protocol:
    def __init__(self):
        self.handlers = []

    def add_handler(self, *, encoder = None, decoder = None, work_builder = None):

        if encoder is None:
            encoder = lambda x: taskloaf.serialize.dumps(x)

        if decoder is None:
            decoder = lambda w, b: taskloaf.serialize.loads(w, b)

        if work_builder is None:
            work_builder = lambda x: x

        self.handlers.append((encoder, decoder, work_builder))
        return len(self.handlers) - 1

    def encode(self, type, obj):
        b = self.handlers[type][0](obj)
        return pickle.dumps((type, b))

    def decode(self, w, bytes):
        type, remaining = pickle.loads(bytes)
        return self.build_work(
            type,
            self.handlers[type][1](w, remaining)
        )

    def build_work(self, type, x):
        return self.handlers[type][2](x)

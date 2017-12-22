import struct
import time

import taskloaf.serialize

def default_encoder(*args):
    return bytes(8) + taskloaf.serialize.dumps(args)

def default_decoder(w, bytes_list):
    return taskloaf.serialize.loads(w, bytes_list)

def default_work_builder(x):
    return x[0]

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
        out = self.handlers[type_code][0](*args)
        if type(out) is bytes:
            out = memoryview(bytearray(out))
        struct.pack_into('l', out, 0, type_code)
        return out

    def decode(self, w, bytes):
        type_code = struct.unpack_from('l', bytes)[0]
        return type_code, self.handlers[type_code][1](w, memoryview(bytes)[8:])

    def build_work(self, type_code, x):
        return self.handlers[type_code][2](x)

    def get_name(self, type_code):
        return self.handlers[type_code][3]

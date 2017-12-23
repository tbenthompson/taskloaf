import struct
import time

import taskloaf.serialize

def get_fmt(t):
    if t == int:
        return 'l'
    elif t == bytes:
        return 'v'

def get_bytes_idxs(fmt):
    return [i for i, char in enumerate(fmt) if char == 'v']

def bytes_to_length(fmt):
    return fmt.replace('v', 'l')

class SimplePack:
    def __init__(self, type_list):
        self.fmt = ''.join([get_fmt(t) for t in type_list])
        self.bytes_idxs = get_bytes_idxs(self.fmt)
        self._packer = struct.Struct(bytes_to_length(self.fmt))

    def pack(self, *args):
        n_total_bytes = self._packer.size
        packer_args = []
        for i in range(len(args)):
            if i in self.bytes_idxs:
                n_bytes = len(args[i])
                packer_args.append(n_bytes)
                n_total_bytes += n_bytes
            else:
                packer_args.append(args[i])

        out = memoryview(bytearray(n_total_bytes))
        self._packer.pack_into(out, 0, *packer_args)

        next_byte_idx = self._packer.size
        for idx in self.bytes_idxs:
            start_idx = next_byte_idx
            next_byte_idx += packer_args[idx]
            out[start_idx:next_byte_idx] = args[idx]
        return out

    def unpack(self, b):
        out = list(self._packer.unpack_from(b))

        next_byte_idx = self._packer.size
        for idx in self.bytes_idxs:
            start_idx = next_byte_idx
            next_byte_idx += out[idx]
            out[idx] = b[start_idx:next_byte_idx]
        return out



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

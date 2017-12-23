import uuid
from taskloaf.timer import Timer
import taskloaf.memory
import taskloaf.protocol
import pyarrow
import struct

import cppimport.import_hook
import serialize

import capnp
import simple_capnp

def benchmark_encode():
    args = [0, 1, 2, 3]
    n = int(5e5)
    t = Timer()
    # for i in range(n):
    #     res = taskloaf.memory.decref_encoder(*args)
    # t.report('encode')
    # for i in range(n):
    #     byt = bytearray(40)
    #     serialize.f(byt, 123, 456, 789, 1010, 2020)
    # t.report('extension')
    # s = struct.Struct('lllll')
    # for i in range(n):
    #     byt = s.pack(123, 456, 789, 1010, 2020)
    # t.report('struct')
    for i in range(n):
        obj = simple_capnp.Simple.new_message()
        obj.owner = 123
        obj.creator = 456
        obj.id = 789
        obj.gen = 1010
        obj.nchildren = 2020
    t.report('capnp')
    sp = taskloaf.protocol.SimplePack([int,int,int,int,int])
    for i in range(n):
        sp.pack(123, 456, 789, 1010, 2020)
    t.report('simplepack')
    #     byt = pyarrow.array([123, 456, 789, 1010, 2020])


if __name__ == "__main__":
    benchmark_encode()

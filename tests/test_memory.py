import uuid
from taskloaf.timer import Timer
import taskloaf.memory
import taskloaf.protocol
import pyarrow
import struct

import cppimport.import_hook
import serialize

def benchmark_encode():
    args = [1, uuid.uuid4(), 2, 3]
    n = int(5e5)
    t = Timer()
    for i in range(n):
        res = taskloaf.memory.decref_encoder(*args)
    t.report('encode')
    for i in range(n):
        byt = bytearray(40)
        serialize.f(byt, 123, 456, 789, 1010, 2020)
    t.report('encode')
    for i in range(n):
        byt = struct.pack('lllll', 123, 456, 789, 1010, 2020)
    t.report('encode')

if __name__ == "__main__":
    benchmark_encode()

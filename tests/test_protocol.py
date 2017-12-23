import numpy as np
import pyarrow
from taskloaf.protocol import Protocol, get_fmt, SimplePack
import struct

def test_roundtrip():
    p = Protocol()
    p.add_handler('simple')
    data = (123, 456)
    b = p.encode(p.simple, *data)
    out = p.decode(None, b)
    assert(out == (p.simple, data))

def test_work():
    p = Protocol()
    p.add_handler('simple')
    assert(p.get_name(p.simple) == 'simple')

    def set():
        set.val = 1
    set.val = 0
    p.build_work(0, [set])()
    assert(set.val == 1)

class FakeWorker:
    def __init__(self):
        self.next_id = 0
        self.addr = 0

    def get_id(self):
        self.next_id += 1
        return self.next_id - 1

def test_get_fmt():
    assert(get_fmt(int) == 'l')
    assert(get_fmt(bytes) == 'v')

def test_pack():
    packer = SimplePack([int])
    b = packer.pack(13)
    assert(packer.fmt == 'l')
    assert(struct.unpack(packer.fmt, b)[0] == 13)

def test_pack_bytes():
    packer = SimplePack([int, bytes])
    assert(packer.fmt == 'lv')
    A = np.random.rand(100)
    b = packer.pack(13, pyarrow.serialize(A).to_buffer().to_pybytes())
    orig = packer.unpack(b)
    assert(orig[0] == 13)
    np.testing.assert_almost_equal(A, pyarrow.deserialize(orig[1]))


# def test_capnp_await():
#     from taskloaf.promise import Promise, Ref
#     ref = Ref(FakeWorker(), 1)
#
#     import capnp
#     import simple_capnp
#     m = simple_capnp.Simple.new_message()
#     m.owner = ref.owner
#     m.creator = ref.creator
#     m.id = ref._id
#     m.gen = ref.gen
#     m.nchildren = ref.n_children
#     m.reqaddr = 1

from taskloaf.protocol import *
from taskloaf.ref import put, ObjectRefListMsg
from taskloaf.run import null_comm_worker
from fixtures import w

def test_roundtrip_default(w):
    p = Protocol()
    p.add_msg_type('simple')
    data = (123, 456)
    b = p.encode(w, p.simple, data)
    out = p.decode(w, b)
    assert(out[1] == data)

def test_work(w):
    p = Protocol()
    p.add_msg_type('simple', handler = lambda w, x: x())
    assert(p.get_name(p.simple) == 'simple')
    def f():
        f.val = 1
    f.val = 0
    p.handle(w, 0, f)
    assert(f.val == 1)

def test_encode_decode(w):
    p = Protocol()
    p.add_msg_type('simple', type = ObjectRefListMsg)
    refs = [put(w, w.addr + 1).convert() for i in range(3)]
    b = p.encode(w, p.simple, refs)
    type_code, new_refs = p.decode(w, b)
    for dr1, dr2 in zip(refs, new_refs):
        assert(dr1.ref._id == dr2.ref._id)

from taskloaf.protocol import *
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

from taskloaf.refcounting import *
from fixtures import w

def test_refcount_simple():
    rc = RefCount()
    assert(rc.alive())
    rc.dec_ref(0, 0)
    assert(not rc.alive())

def test_refcount_dead():
    rc = RefCount()
    rc.dec_ref(0, 2)
    rc.dec_ref(1, 0)
    rc.dec_ref(1, 0)
    assert(not rc.alive())

def test_refcount_alive():
    rc = RefCount()
    rc.dec_ref(0, 2)
    rc.dec_ref(1, 0)
    rc.dec_ref(1, 1)
    assert(rc.alive())

def test_decref_encode(w):
    b = DecRefMsg.serialize([2, 3, 4]).to_bytes()
    m = taskloaf.message_capnp.Message.from_bytes(b)
    _id, gen, n_children = DecRefMsg.deserialize(w, m)
    assert(_id == 2)
    assert(gen == 3)
    assert(n_children == 4)

import gc

from taskloaf.memory import *
from taskloaf.run import null_comm_worker

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

def test_put_get_delete():
    w = null_comm_worker()
    mm = w.memory
    dref = DistributedRef(w, w.addr + 1)
    mm.put(1, dref = dref)
    assert(mm.available(dref))
    assert(mm.get(dref) == 1)
    mm.delete(dref)
    assert(not mm.available(dref))

def test_decref_local():
    w = null_comm_worker()
    mm = w.memory
    dref = mm.put(1)
    assert(len(mm.blocks.keys()) == 1)
    del dref
    gc.collect() # Force a GC collect to make sure that dref.__del__ is called
    assert(len(mm.blocks.keys()) == 0)

def test_decref_encode():
    w = null_comm_worker()
    coded = decref_encoder(1, 2, 3, 4)
    creator, _id, gen, n_children = decref_decoder(w, memoryview(coded)[8:])
    assert(creator == 1)
    assert(_id == 2)
    assert(gen == 3)
    assert(n_children == 4)

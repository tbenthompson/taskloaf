import pickle
import pytest
from taskloaf.refcounting import *
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

def test_make_ref():
    with null_comm_worker() as w:
        r = Ref(w, [12,3,4])
        assert(r.get() == [12,3,4])

def test_gcref_get():
    with null_comm_worker() as w:
        r = Ref(w, [12,3,4])
        gcr = r.convert()
        assert(gcr.get() == [12,3,4])
        w.object_cache.clear()
        assert(gcr.get() == [12,3,4])

def test_ref_conversion_caching():
    with null_comm_worker() as w:
        r = Ref(w, [12,3,4])
        gcr = r.convert()
        assert(isinstance(gcr, GCRef))
        gcr2 = r.convert()
        assert(gcr is gcr2)

def test_ref_pickle_exception():
    with null_comm_worker() as w:
        r = Ref(w, [12,3,4])
        gcr = r.convert()

        with pytest.raises(Exception):
            pickle.dumps(r)

        with pytest.raises(Exception):
            pickle.dumps(gcr)
